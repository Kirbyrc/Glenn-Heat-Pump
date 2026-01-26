//#include <Arduino.h>
#include "hp_emulator.h"


namespace HVAC {

 // Constructor initializes variables with default values
 HPEmulator::HPEmulator() : 
    _power(1), _mode(2), _fan_speed(3), _set_temp(20), _act_temp(18),
    _vane_vertical(3), _vane_horizontal(1) {
    }

// --- Setters ---
void HPEmulator::setPower(uint8_t value)          { _power = value; }
void HPEmulator::setMode(uint8_t value)           { _mode = value; }
void HPEmulator::setFanSpeed(uint8_t value)       { _fan_speed = value; }
void HPEmulator::setTargetTemp(uint8_t value)     { _set_temp = value; }
void HPEmulator::setActualTemp(uint8_t value)     { _act_temp = value; }
void HPEmulator::setVaneVertical(uint8_t value)   { _vane_vertical = value; }
void HPEmulator::setVaneHorizontal(uint8_t value) { _vane_horizontal = value; }

// --- Getters ---
uint8_t HPEmulator::getPower() const          { return _power; }
uint8_t HPEmulator::getMode() const           { return _mode; }
uint8_t HPEmulator::getFanSpeed() const       { return _fan_speed; }
uint8_t HPEmulator::getTargetTemp() const     { return _set_temp; }
uint8_t HPEmulator::getActualTemp() const     { return _act_temp; }
uint8_t HPEmulator::getVaneVertical() const   { return _vane_vertical; }
uint8_t HPEmulator::getVaneHorizontal() const { return _vane_horizontal; }

// variables
DataBuffer Stim_buffer; //used to build stimulus
DataBuffer Remote_buffer; //used to receive from remote

// --- Logic Methods ---

void HPEmulator::print_utility(char* data) {
  // int len=strlen(data);
  // Serial.printf("PU: %d - %s\n",len,data);
  Serial.print(data);
  // if (Config::telnetClient && Config::telnetClient.connected()) {
  //   Config::telnetClient.print(data);  
  //  }
  } 
  
void HPEmulator::print_byte(uint8_t byte, const char* mess1, const char* mess2="") {
    char buf[256];
    unsigned long currentMS = millis();
    snprintf(buf, 256, "(%08d-000) %s %s 0x%02x\n", currentMS, mess1, mess2, byte);  HPEmulator::print_utility(buf);
 }

void HPEmulator::print_packet(struct DataBuffer* dbuf, const char* mess1, const char* mess2) { 
    char buf[256];
    int i;
    unsigned long currentMS = millis();
    snprintf(buf, 256, "(%08d-000) %s %s: ", currentMS, mess1, mess2);  print_utility(buf);
    for (i=0; i<dbuf->buf_pointer; i++ ) {
        if (i%4==0) snprintf(buf, 256, " %02x", dbuf->buffer[i]); 
        else snprintf(buf, 256, "%02x", dbuf->buffer[i]); 
        print_utility(buf);
    }
    snprintf(buf, 256, "%s", "\n");  print_utility(buf);
    }

bool HPEmulator::check_checksum(struct DataBuffer* dbuf) {
    int i;
    uint8_t packetCheckSum = dbuf->buffer[dbuf->buf_pointer-1];
    uint8_t processedCS = 0;
    //print_byte(packetCheckSum,"Packet Checksum in", dbuf->bufname);
      for (i=0; i<dbuf->length-1; i++) {
        //print_byte(dbuf->buffer[i],"Building Checksum in", dbuf->bufname);
        processedCS += dbuf->buffer[i];
        }
    processedCS = (0xfc - processedCS) & 0xff;
    //print_byte(processedCS,"Processed Checksum in", dbuf->bufname);
    return (packetCheckSum == processedCS);
  }

void HPEmulator::add_checksum_to_packet(struct DataBuffer* dbuf) {
    int i;
    uint8_t processedCS=0;
    for (i=0; i<dbuf->length-1; i++) {
      //print_byte(dbuf->buffer[i],"Building Checksum ", dbuf->bufname);
      processedCS += dbuf->buffer[i];
      }
    dbuf->buffer[dbuf->length-1] = (0xfc - processedCS) & 0xff;
    //print_byte(dbuf->buf_pointer,"Buf Pointer in", dbuf->bufname);
    //(dbuf, "Completed Packet in", dbuf->bufname);
  }

const char* HPEmulator::lookupByteMapValue(const char* const valuesMap[], const byte byteMap[], int len, byte byteValue) {
  for (int i = 0; i < len; i++) {
    if (byteMap[i] == byteValue) {
      return valuesMap[i];
    }
  }
  return valuesMap[0];
}

int HPEmulator::lookupByteMapValue(const int valuesMap[], const byte byteMap[], int len, byte byteValue) {
  for (int i = 0; i < len; i++) {
    if (byteMap[i] == byteValue) {
      return valuesMap[i];
    }
  }
  return valuesMap[0];
}

void HPEmulator::send_ping_reponse_to_remote(struct DataBuffer* dbuf, HardwareSerial &serial_port) {
    char buf[256];
    Stim_buffer.buf_pointer=sizeof(PING_RESPONSE);
    Stim_buffer.length=Stim_buffer.buf_pointer;    
    memcpy(Stim_buffer.buffer, PING_RESPONSE, sizeof(PING_RESPONSE));
    add_checksum_to_packet(&Stim_buffer);
    print_packet(&Stim_buffer, "Packet to", "RE");
    serial_port.write(Stim_buffer.buffer, Stim_buffer.buf_pointer);
   }

void HPEmulator::send_remote_state_to_heatpump(struct DataBuffer* dbuf, HardwareSerial &serial_port){
    char buf[256];
    
    uint8_t mask1 = dbuf->buffer[6];
    uint8_t mask2 = dbuf->buffer[7];
    // Serial.printf("mask1 - %0x\n", mask1);
    // Serial.printf("mask2 - %0x\n", mask2);

    if (mask1 & 0x01) {
       setPower(dbuf->buffer[8]);
       //Serial.printf("Setting power - %0x\n", dbuf->buffer[8]);
      }
    if (mask1 & 0x02) {
       setMode(dbuf->buffer[9]);
       //Serial.printf("Setting mode - %0x\n", dbuf->buffer[9]);
      }
    if (mask1 & 0x04) {
       uint8_t temp = (dbuf->buffer[19]&0x7f)>>1;
       setTargetTemp(temp);
       setActualTemp(temp-2); // For simplicity, set actual temp to target temp minus 2 degrees
       //Serial.printf("Setting target/actual temp - %0x\n", temp);
      }
    if (mask1 & 0x08) {
       setFanSpeed(dbuf->buffer[11]);
       //Serial.printf("Setting fan speed - %0x\n", dbuf->buffer[11]);
      } 
    if (mask1 & 0x10) {
       setVaneVertical(dbuf->buffer[12]);
       Serial.printf("Setting vane vertical - %0x\n", dbuf->buffer[12]);
      }
    if (mask2 & 0x01) {
       setVaneHorizontal(dbuf->buffer[18]);
       Serial.printf("Setting vane horizontal - %0x\n", dbuf->buffer[18]);
      } 

    Stim_buffer.buf_pointer=sizeof(CONTROL_RESPONSE);
    Stim_buffer.length=Stim_buffer.buf_pointer;    
    memcpy(Stim_buffer.buffer, CONTROL_RESPONSE, sizeof(CONTROL_RESPONSE));
    add_checksum_to_packet(&Stim_buffer);
    print_packet(&Stim_buffer, "Packet to", "RE");
    serial_port.write(Stim_buffer.buffer, Stim_buffer.buf_pointer);
   }

void HPEmulator::send_heatpump_state_to_remote(struct DataBuffer* dbuf, HardwareSerial &serial_port){
    char buf[256];
    Stim_buffer.buf_pointer=sizeof(INFO_RESPONSE);
    Stim_buffer.length=Stim_buffer.buf_pointer; 
    memcpy(Stim_buffer.buffer, INFO_RESPONSE, sizeof(INFO_RESPONSE));
    uint8_t info_mode=dbuf->buffer[5];
    Stim_buffer.buffer[5]=info_mode;
    //print_byte(info_mode, "Info Mode", dbuf->bufname);

    switch(info_mode) {
      case 0x02: {
         //settings request
         //Serial.println("Settings Request");
         Stim_buffer.buffer[8]=getPower();
         Stim_buffer.buffer[9]=getMode();
         Stim_buffer.buffer[10]=getTargetTemp();
         Stim_buffer.buffer[11]=getFanSpeed();
         Stim_buffer.buffer[12]=getVaneVertical();
         Stim_buffer.buffer[14]=getVaneHorizontal();
         Stim_buffer.buffer[16]=(getTargetTemp()<<1)| 0x80; //target temp in bits 1-7
         break;
      }
      case 0x03: {
         // room temp request
         //Serial.println("Room Temp Request");
         Stim_buffer.buffer[11]=(getActualTemp()<<1)| 0x80; //target temp in bits 1-7
         break;
      }
      case 0x04: {
        //unknown request
        //Serial.println("Unknown Request");
        break;
      }
      case 0x05: {
        //timer request
        //Serial.println("Timer Request");
        break;
      }
      case 0x06: {
         //status request
         //Serial.println("Status Request");
         break;
      }
      case 0x09: {
        //standby mode request
        //Stim_buffer.buffer[9]=1;
        //Serial.println("Standby Mode Request");
        break;
      }
    }
    
    add_checksum_to_packet(&Stim_buffer);
    print_packet(&Stim_buffer, "Packet to", "RE");
    serial_port.write(Stim_buffer.buffer, Stim_buffer.buf_pointer);
}

void HPEmulator::process_packets(struct DataBuffer* dbuf, HardwareSerial &serial_port) {
    char buf[256];
   //print the incoming packet
   print_packet(dbuf, "Packet to", "HP");
  
   uint8_t cmd = dbuf->buffer[1];
   //print_byte(cmd, "Packet Command", dbuf->bufname);

  if (cmd==0x5a) this->send_ping_reponse_to_remote(dbuf, serial_port);
  else if (cmd==0x41) this->send_remote_state_to_heatpump(dbuf, serial_port);
  else if (cmd==0x42) this->send_heatpump_state_to_remote(dbuf, serial_port);
}

void HPEmulator::check_header(struct DataBuffer* dbuf) {
  if (dbuf->buf_pointer == 5) {
    if (dbuf->buffer[2] == HEADER[2] && dbuf->buffer[3] == HEADER[3]) {
      dbuf->command = dbuf->buffer[1];
      dbuf->length = dbuf->buffer[4]+6; //length byte + header size + checksum
      //print_byte(dbuf->command,"Command Identified in", dbuf->bufname);
      //print_byte(dbuf->length,"Packet Length in", dbuf->bufname);
      }
    }
  }

void HPEmulator::process_port_emulator(struct DataBuffer* dbuf, HardwareSerial &read_serial) {
  while (read_serial.available()) {
    uint8_t S1byte = read_serial.read(); //read a byte from the heatpump
    //print_byte(S1byte,"Byte Read into", dbuf->bufname);
    if (!dbuf->foundStart){
        if (S1byte==HEADER[0]) { //start of packet
          //print_byte(S1byte,"Found Packet in", dbuf->bufname);
          dbuf->foundStart = true;
          dbuf->length = 22; //default length until we parse the real length
          dbuf->buffer[dbuf->buf_pointer++] = S1byte;
          }
        }
    else {  //building a packet
      //print_byte(S1byte,"Building packet in", dbuf->bufname);
      dbuf->buffer[dbuf->buf_pointer++] = S1byte;
      check_header(dbuf);  //assign length and command if we have enough data
      //print_byte(dbuf->length,"Length in", dbuf->bufname);
      //print_byte(dbuf->buf_pointer,"buf_pointer in", dbuf->bufname);
      if (dbuf->buf_pointer >= dbuf->length) {
           if (check_checksum(dbuf)) { 
              //valid packet
              process_packets(dbuf, read_serial);
              }
            else print_packet(dbuf, "BAD Packet to", "HP");
          dbuf->foundStart = false;
          dbuf->buf_pointer = 0;
          }
      }
    }
  }

void HPEmulator::setup() {
  Serial.println("Initializing Class: HPEmulator");

};

void HPEmulator::loop() {
  process_port_emulator(&Remote_buffer, RE_SERIAL);
  };


} // namespace HVAC

