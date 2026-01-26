
#pragma once

//#include <Arduino.h>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <string>

#define HP_SERIAL Serial1
#define RE_SERIAL Serial2

namespace HVAC {

// --- Structs ---
struct DataBuffer {
    uint8_t buffer[256]; //the packet data
    uint8_t buf_pointer; //pointer to the next position in the buffer
    bool foundStart; //determines that we found a packet start character and are now building a packet
    uint8_t command; //This is the packet command 
    uint8_t length; //This is the length of the packet
    };
        
class HPEmulator {
public:
    HPEmulator(); // Constructor

    // --- Static Constants

    const int RE_TX_PIN = 11; // TX2
    const int RE_RX_PIN = 10; // RX2
    const int BAUD = 2400;
    
    const uint8_t HEADER[5] = { 0xfc, 0x42, 0x01, 0x30, 0x10 };
    const uint8_t COMMANDS[6] = { 0x5a, 0x42, 0x41, 0x7a, 0x62, 0x61};
    const uint8_t PING_RESPONSE[7] = { 0xfc, 0x7a, 0x01, 0x30, 0x01, 0x00, 0x54 };
    const uint8_t INFO_RESPONSE[22] = { 0xfc, 0x62, 0x01, 0x30, 0x10, 0x00, 0x00, 0x00, 
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    const uint8_t CONTROL_RESPONSE[22] = { 0xfc, 0x61, 0x01, 0x30, 0x10, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

    const uint8_t POWER[2]            = {0x00, 0x01};
    const char* POWER_MAP[2]       = {"OFF", "ON"};
    const uint8_t MODE[5]             = {0x01,   0x02,  0x03, 0x07, 0x08};
    const char* MODE_MAP[5]        = {"HEAT", "DRY", "COOL", "FAN", "AUTO"};
    const uint8_t TEMP[16]            = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};
    const int TEMP_MAP[16]         = {31, 31, 29, 28, 27, 26, 25, 24, 23, 22, 21, 21.5f};
    const uint8_t FAN[6]              = {0x00,  0x01,   0x02, 0x03, 0x05, 0x06};
    const char* FAN_MAP[6]         = {"AUTO", "QUIET", "1", "2", "3", "4"};
    const uint8_t VANE[7]             = {0x00,  0x01, 0x02, 0x03, 0x04, 0x05, 0x07};
    const char* VANE_MAP[7]        = {"AUTO", "1", "2", "3", "4", "5", "SWING"};
    const uint8_t WIDEVANE[7]         = {0x01, 0x02, 0x03, 0x04, 0x05, 0x8,  0xc};
    const char* WIDEVANE_MAP[7]    = {"<<", "<", "|", ">", ">>", "<>", "SWING"};
    const uint8_t ROOM_TEMP[32]       = {   16,   17,   18,   19,   21,
                                      };
    const int ROOM_TEMP_MAP[32]    = {16,   17,   18,   19,
                                      };
    const uint8_t TIMER_MODE[4]       = {0x00,  0x01,  0x02, 0x03};
    const char* TIMER_MODE_MAP[4]  = {"NONE", "OFF", "ON", "BOTH"};


    // --- Setters ---
    void setPower(uint8_t value);
    void setMode(uint8_t value);
    void setFanSpeed(uint8_t value);
    void setTargetTemp(uint8_t value);
    void setActualTemp(uint8_t value);
    void setVaneVertical(uint8_t value);
    void setVaneHorizontal(uint8_t value);

    // --- Getters ---
    uint8_t getPower() const;
    uint8_t getMode() const;
    uint8_t getFanSpeed() const;
    uint8_t getTargetTemp() const;
    uint8_t getActualTemp() const;
    uint8_t getVaneVertical() const;
    uint8_t getVaneHorizontal() const;

    // --- Logic Methods ---
    const char* lookupByteMapValue(const char* const valuesMap[], const byte byteMap[], int len, byte byteValue);
    int  lookupByteMapValue(const int valuesMap[], const byte byteMap[], int len, byte byteValue);
    void emulate_heatpump(struct DataBuffer* dbuf, HardwareSerial &serial_port);
    void send_ping_reponse_to_remote(struct DataBuffer* dbuf, HardwareSerial &serial_port);
    void send_remote_state_to_heatpump(struct DataBuffer* dbuf, HardwareSerial &serial_port);
    void send_heatpump_state_to_remote(struct DataBuffer* dbuf, HardwareSerial &serial_port);
    void print_utility(char* data);
    void print_byte(uint8_t byte, const char* mess1, const char* mess2);
    void print_packet(struct DataBuffer* dbuf, const char* mess1, const char* mess2);
    void add_checksum_to_packet(struct DataBuffer* dbuf);
    bool check_checksum(struct DataBuffer* dbuf);
    void check_header(struct DataBuffer* dbuf);
    void process_packets(struct DataBuffer* dbuf, HardwareSerial &serial_port);
    void process_port_emulator(struct DataBuffer* dbuf, HardwareSerial &read_serial);
    void setup();
    void loop();

private:
    uint8_t _power;
    uint8_t _mode;
    uint8_t _fan_speed;
    uint8_t _set_temp;
    uint8_t _act_temp;
    uint8_t _vane_vertical;
    uint8_t _vane_horizontal;
};

} // namespace HVAC
