
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "driver/uart.h"
#include "esp_http_server.h"

#define HP_UART_NUM UART_NUM_1
#define RE_UART_NUM UART_NUM_2

// Forward declaration for CN105Climate
namespace esphome {
    class CN105Climate;
}

// Global pointer to CN105Climate - set from CN105Climate::setup()
extern esphome::CN105Climate* g_cn105;

namespace HVAC {

const int RE_TX2_PIN = 11; // TX2
const int RE_RX2_PIN = 10; // RX2
const int HP_TX1_PIN = 13; // TX1
const int HP_RX1_PIN = 12; // RX1
const int BAUD = 2400;

const int WEBPORT = 81;

// --- Structs ---
struct DataBuffer {
    uint8_t buffer[256]; //the packet data
    uint8_t buf_pointer; //pointer to the next position in the buffer
    bool foundStart; //determines that we found a packet start character and are now building a packet
    uint8_t command; //This is the packet command
    uint8_t length; //This is the length of the packet
};

class HPEmulator {
    friend esp_err_t heatpump_status_handler(httpd_req_t *req);
public:
    HPEmulator(); // Constructor

    // --- Static Constants
    const uint8_t HEADER[5] = { 0xfc, 0x42, 0x01, 0x30, 0x10 };
    const uint8_t COMMANDS[6] = { 0x5a, 0x42, 0x41, 0x7a, 0x62, 0x61};

    const uint8_t PING_RESPONSE[7] = { 0xfc, 0x7a, 0x01, 0x30, 0x01, 0x00, 0x54 };
    const uint8_t INFO_RESPONSE[22] = { 0xfc, 0x62, 0x01, 0x30, 0x10, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    const uint8_t CONTROL_RESPONSE[22] = { 0xfc, 0x61, 0x01, 0x30, 0x10, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    const uint8_t CONFIG_RESPONSE[22] = { 0xfc, 0x7b, 0x01, 0x30, 0x10, 0xc9, 0x03, 0x00,
        0x20, 0x00, 0x14, 0x07, 0x75, 0x0c, 0x05, 0xa0, 0xbe, 0x94, 0xbe, 0xa0, 0xbe, 0xa9 };

    const uint8_t POWER[2]            = {0x00, 0x01};
    const char* POWER_MAP[2]       = {"OFF", "ON"};
    const uint8_t MODE[5]             = {0x01,   0x02,  0x03, 0x07, 0x08};
    const char* MODE_MAP[5]        = {"HEAT", "DRY", "COOL", "FAN", "AUTO"};
    const uint8_t TEMP[16]            = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};
    const int TEMP_MAP[16]         = {31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16};
    const uint8_t FAN[6]              = {0x00,  0x01,   0x02, 0x03, 0x05, 0x06};
    const char* FAN_MAP[6]         = {"AUTO", "QUIET", "1", "2", "3", "4"};
    const uint8_t VANE[7]             = {0x00,  0x01, 0x02, 0x03, 0x04, 0x05, 0x07};
    const char* VANE_MAP[7]        = {"AUTO", "1", "2", "3", "4", "5", "SWING"};
    const uint8_t WIDEVANE[7]         = {0x01, 0x02, 0x03, 0x04, 0x05, 0x08, 0x0c};
    const char* WIDEVANE_MAP[7]    = {"<<", "<",  "|",  ">",  ">>", "<>", "SWING"};
    const uint8_t ROOM_TEMP[32]       = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
                                      0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f};
    const int ROOM_TEMP_MAP[32]    = {10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,
                                      26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41};
    const uint8_t TIMER_MODE[4]       = {0x00,  0x01,  0x02, 0x03};
    const char* TIMER_MODE_MAP[4]  = {"NONE", "OFF", "ON", "BOTH"};

    // --- Initialization ---
    void stateInit(uint8_t power, uint8_t mode, uint8_t fan_speed,
              uint8_t set_temp, uint8_t act_temp, uint8_t vane_vertical,
              uint8_t vane_horizontal);
    
    // --- Primary Entry Points ---
    void setup();
    void run();

    // --- Setters ---
    bool setPower(uint8_t value);
    bool setMode(uint8_t value);
    void setFanSpeed(uint8_t value);
    void setTargetTemp(uint8_t value);
    void setActualTemp(uint8_t value);
    void setVaneVertical(uint8_t value);
    void setVaneHorizontal(uint8_t value);

    // --- Comparison ---
    void getEsphomeState();
    bool compareWithESPHOME() const;

    // --- Logic Methods ---
    const char* lookupByteMapValue(const char* const valuesMap[], const uint8_t byteMap[], int len, uint8_t byteValue);
    int  lookupByteMapValue(const int valuesMap[], const uint8_t byteMap[], int len, uint8_t byteValue);
    int  lookupByteMapIndex(const char* valuesMap[], int len, const char* lookupValue);
    void send_ping_response_to_remote(struct DataBuffer* dbuf, uart_port_t uart_num);
    void send_config_response_to_remote(struct DataBuffer* dbuf, uart_port_t uart_num);
    void send_remote_state_to_heatpump(struct DataBuffer* dbuf, uart_port_t uart_num);
    void send_heatpump_state_to_remote(struct DataBuffer* dbuf, uart_port_t uart_num);
    void print_utility(const char* data);
    void print_byte(uint8_t byte, const char* mess1, const char* mess2);
    void print_packet(struct DataBuffer* dbuf, const char* mess1, const char* mess2);
    void add_checksum_to_packet(struct DataBuffer* dbuf);
    bool check_checksum(struct DataBuffer* dbuf);
    void check_header(struct DataBuffer* dbuf);
    void process_packets(struct DataBuffer* dbuf, uart_port_t uart_num);
    void process_port_emulator(struct DataBuffer* dbuf, uart_port_t uart_num);
    void* start_webserver();
    bool uartInit();

private:
    // Emulator State Variables
    uint8_t emulatorPower;
    uint8_t emulatorMode;
    uint8_t emulatorFan;
    uint8_t emulatorSetTemp;
    uint8_t emulatorActualTemp;
    uint8_t emulatorVertVane;
    uint8_t emulatorHoriVane;

    // ESPHome state variables
    uint8_t esphomePower;
    uint8_t esphomeMode;
    uint8_t esphomeFan;
    uint8_t esphomeSetTemp;
    uint8_t esphomeActualTemp;
    uint8_t esphomeVertVane;
    uint8_t esphomeHoriVane;

    // Flag to indicate if the webserver has been started
    bool _webserver_started;
};

} // namespace HVAC
