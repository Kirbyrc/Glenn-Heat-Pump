#include <stdio.h>
#include <string.h>
#include "hp_emulator_idf.h"
#include "esphome/components/uart/uart_component_esp_idf.h"
#include "cn105.h"
#include "driver/uart.h"
#include "esphome.h"
#include "esp_timer.h"
#include "esp_http_server.h"
#include "esp_netif.h"

// Global pointer to CN105Climate instance
esphome::CN105Climate* g_cn105 = nullptr;
// Global pointer to RE_UART - set from main.cpp after RE_UART is configured
esphome::uart::IDFUARTComponent* g_re_uart = nullptr;

namespace HVAC {

static const char *TAG = "HPE_Core";

// Helper function to check if all core char* fields in wantedHeatpumpSettings are non-null
// Returns true if power, mode, fan, vane, and wideVane are all set
static bool areWantedSettingsIntialized(const wantedHeatpumpSettings& settings) {
    return (settings.power != nullptr &&
            settings.mode != nullptr &&
            settings.fan != nullptr &&
            settings.vane != nullptr &&
            settings.wideVane != nullptr);
}

static bool areCurrentSettingsIntialized(const heatpumpSettings& settings) {
    return (settings.power != nullptr &&
            settings.mode != nullptr &&
            settings.fan != nullptr &&
            settings.vane != nullptr &&
            settings.wideVane != nullptr);

}

// --- Setters ---
void HPEmulator::setPower(HeatpumpState* state, uint8_t value) {
    if (value > 1) ESP_LOGW(TAG, "Power Out of Range: %d", value);
    else state->power = value; 
    }

void HPEmulator::setMode(HeatpumpState* state, uint8_t value) {
    if (value > 0x08) ESP_LOGW(TAG, "Mode Out of Range: %d", value);
    else state->mode = value;
    }

void HPEmulator::setFanSpeed(HeatpumpState* state, uint8_t value) {
    if (value > 6) ESP_LOGW(TAG, "Fan Speed Out of Range: %d", value);
    else state->fan = value;
    }

void HPEmulator::setTargetTemp(HeatpumpState* state, uint8_t value) {
    if (value < 0x10) ESP_LOGW(TAG, "Target Temp Out of Range: %d", value);
    else state->setTemp = value;
    }

void HPEmulator::setActualTemp(HeatpumpState* state, uint8_t value) {
    if (value < 0x10) ESP_LOGW(TAG, "Actual Temp Out of Range: %d", value);
    else state->actualTemp = value;
    }

void HPEmulator::setVaneVertical(HeatpumpState* state, uint8_t value) {
    if (value > 7) ESP_LOGW(TAG, "Vane Vertical Out of Range: %d", value);
    else state->vertVane = value;
    }

void HPEmulator::setVaneHorizontal(HeatpumpState* state, uint8_t value) {
    if (value > 12) ESP_LOGW(TAG, "Vane Horizontal Out of Range: %d", value);
    else state->horiVane = value;
    }

void HPEmulator::debugHPState(const char* label, const HeatpumpState& state) {
    ESP_LOGD(TAG, "[%s]-> [power: %s, mode: %s, fan: %s, setTemp: %d, actualTemp: %d, vane: %s, wideVane: %s]",
        label,
        lookupByteMapValue(POWER_MAP, POWER, 2, state.power),
        lookupByteMapValue(MODE_MAP, MODE, 5, state.mode),
        lookupByteMapValue(FAN_MAP, FAN, 6, state.fan),
        state.setTemp,
        state.actualTemp,
        lookupByteMapValue(VANE_MAP, VANE, 7, state.vertVane),
        lookupByteMapValue(WIDEVANE_MAP, WIDEVANE, 7, state.horiVane)
        );
}

// --- Pull the state from the esphome code ---
void HPEmulator::getEsphomeStatefromEngine() {
    int index;
    HeatpumpState tempState;
    
    if (g_cn105 == nullptr) {
        ESP_LOGE(TAG, "g_cn105 is null, cannot get ESPHome state");
        return;
        }

    if (!areCurrentSettingsIntialized(g_cn105->currentSettings)) {
        ESP_LOGD(TAG, "Current settings are not initialized");
        return;
        }
    // Print currentSettings from CN105Climate (now public - KIRBY)
    // ESP_LOGD(TAG, "ESPHome currentSettings:");
    // ESP_LOGD(TAG, "  power: %s", g_cn105->currentSettings.power ? g_cn105->currentSettings.power : "null");
    // ESP_LOGD(TAG, "  mode: %s", g_cn105->currentSettings.mode ? g_cn105->currentSettings.mode : "null");
    // ESP_LOGD(TAG, "  temperature: %.1f", g_cn105->currentSettings.temperature);
    // ESP_LOGD(TAG, "  fan: %s", g_cn105->currentSettings.fan ? g_cn105->currentSettings.fan : "null");
    // ESP_LOGD(TAG, "  vane: %s", g_cn105->currentSettings.vane ? g_cn105->currentSettings.vane : "null");
    // ESP_LOGD(TAG, "  wideVane: %s", g_cn105->currentSettings.wideVane ? g_cn105->currentSettings.wideVane : "null");

    // Temperatures
    tempState.setTemp = (uint8_t)g_cn105->currentSettings.temperature;
    tempState.actualTemp = (uint8_t)g_cn105->current_temperature;
    
    // For the others: lookup byte value from string
    if (g_cn105->currentSettings.power) {
        index = lookupByteMapIndex(POWER_MAP, 2, g_cn105->currentSettings.power);
        if (index <0) tempState.power = 0;
        else tempState.power = POWER[index];
        }

    if (g_cn105->currentSettings.mode) {
        index = lookupByteMapIndex(MODE_MAP, 5, g_cn105->currentSettings.mode);
        if (index <0) tempState.mode = 0;
        else {
            tempState.mode = MODE[index];
            }
        }

    if (g_cn105->currentSettings.fan) {
        index = lookupByteMapIndex(FAN_MAP, 6, g_cn105->currentSettings.fan);
        if (index <0) tempState.fan = 0;
        else tempState.fan = FAN[index];
        }

    if (g_cn105->currentSettings.vane) {
        index = lookupByteMapIndex(VANE_MAP, 7, g_cn105->currentSettings.vane);
        if (index <0) tempState.vertVane = 0;
        else tempState.vertVane = VANE[index];
        }

    if (g_cn105->currentSettings.wideVane) {
        index = lookupByteMapIndex(WIDEVANE_MAP, 7, g_cn105->currentSettings.wideVane);
        if (index <0) tempState.horiVane = 0;
        else tempState.horiVane = WIDEVANE[index];
        }
    
    if (tempState != esphomeState) {
        esphomeState = tempState;
        ESP_LOGD(TAG, "Esphome state updated from Esphome Engine:");
        debugHPState("Updated Esphome State from Esphome Engine", esphomeState);
        debugHPState("esphome Engine state ready to be copied to esphomeState", tempState);
        }
    
    // the engine is intialized
    if (engineUpTime ==0) {
        engineUpTime = esp_timer_get_time() / 1000; // Convert microseconds to milliseconds
        ESP_LOGD(TAG, "The Engine is up");
        }   

    }

void HPEmulator::sendEmulatorStateToEngine() {
    if (g_cn105 == nullptr) {
        ESP_LOGE(TAG, "g_cn105 is null, cannot create wanted record");
        return;
        }

    if (!areCurrentSettingsIntialized(g_cn105->currentSettings)) {
        ESP_LOGD(TAG, "Emulator Engine is not up, will try again");
        return;
        }

    if (g_cn105->wantedSettings.hasChanged) {
        ESP_LOGD(TAG, "Another Engine change is in progress, waiting for opportunity");
        return;
        }

    //g_cn105->debugSettings("Wanted Settings at prior to update)", g_cn105->wantedSettings);

    // Now we know the settings match
    g_cn105->wantedSettings.power = lookupByteMapValue(POWER_MAP, POWER, 2, emulatorState.power);
    g_cn105->wantedSettings.mode = lookupByteMapValue(MODE_MAP, MODE, 5, emulatorState.mode);
    g_cn105->wantedSettings.fan = lookupByteMapValue(FAN_MAP, FAN, 6, emulatorState.fan);
    g_cn105->wantedSettings.temperature = float(emulatorState.setTemp);
    g_cn105->wantedSettings.vane = lookupByteMapValue(VANE_MAP, VANE, 7,  emulatorState.vertVane);
    g_cn105->wantedSettings.wideVane = lookupByteMapValue(WIDEVANE_MAP, WIDEVANE, 7, emulatorState.horiVane);
  
    //sending state
    g_cn105->wantedSettings.hasChanged = true;
    g_cn105->debugSettings("Settings Sent to Engine based on Emulator State", g_cn105->wantedSettings);
    }

void HPEmulator::simpleOperation() {
    //calling this will update esphomeState from the Engine and emulator state from the remote
    //used to test to see if you screwed up the serial port logic
    getEsphomeStatefromEngine();
    debugHPState("esphomeState pulled from engine", esphomeState);
    if (remoteState != emulatorState) {
        emulatorState = remoteState;
        debugHPState("Emulator State updated from Remote State", emulatorState);
        }
    }

void HPEmulator::updateEmulatorStateFromEngine() {
    // Compare emulatorState to esphomeState
    // If different, update emulatorState from esphomeState

    // static uint64_t lastComparisonTime = 0;
    // const uint64_t comparisonInterval = 2000; // 2 seconds in milliseconds
    // uint64_t currentTime = esp_timer_get_time() / 1000; // Convert microseconds to millisecond
    getEsphomeStatefromEngine();
    if (emulatorState != esphomeState) {
        emulatorState = esphomeState;
        debugHPState("Emulator State updated from Esphome State", emulatorState);
        }  

    // don't update current state until 2 seconds after Engine was written
    // if ((currentTime - remoteLastUpdateTime) >= comparisonInterval) {
    //     getEsphomeStatefromEngine();
    //     if (emulatorState != esphomeState) {
    //         emulatorState = esphomeState;
    //         debugHPState("Emulator State updated from Esphome State", emulatorState);
    //         }  
    //     lastComparisonTime = currentTime;
    //     }
    }    
     
void HPEmulator::checkForRemoteStateChange() {
    // Compare remoteState to emulatorState
    // If different, initial remoteInControl and remoteLastUpdateTime
    // If different, update, esphomeState and esphome engine
    // Set timer so that remoteinControl will stay for 5 seconds
       
    static HeatpumpState lastRemoteState;
    uint64_t currentTime = esp_timer_get_time() / 1000; // Convert microseconds to milliseconds 

    if (remoteState != lastRemoteState && !remoteInControl && systemUP) {
        ESP_LOGD(TAG, "Remote state change detected, remoteInControl set to true.");
        debugHPState("Remote State Value", remoteState);
        debugHPState("Last Remote State Value", lastRemoteState);
        remoteInControl = true;
        remoteLastUpdateTime = currentTime; 
        emulatorState = remoteState;
        esphomeState = remoteState;
        lastRemoteState = remoteState;
        sendEmulatorStateToEngine();
        debugHPState("Emulator/Esphome State updated from Remote State", emulatorState);
        }
    
    const uint64_t comparisonInterval = 30000; // 30 seconds in milliseconds
    static uint64_t lastComparisonTime = 0;
    if ((currentTime - remoteLastUpdateTime) >= comparisonInterval) {
        if (remoteInControl) {
            remoteInControl = false;
            ESP_LOGD(TAG, "Cleared remoteInControl.");
            }   
        lastComparisonTime = currentTime;
        }
    }

void HPEmulator::print_packet(struct DataBuffer* dbuf, const char* mess1, const char* mess2) {
    char buf[256];  // Large enough for timestamp + header + all bytes
    int offset = 0;
    int i;
    //uint64_t currentMS = esp_timer_get_time() / 1000; // Convert microseconds to milliseconds

    // Build timestamp and header
    //offset += snprintf(buf + offset, sizeof(buf) - offset, "(%08lld-000) %s %s: ", currentMS, mess1, mess2);
    offset += snprintf(buf + offset, sizeof(buf) - offset, "%s %s: ", mess1, mess2);

    // Build packet bytes
    for (i = 0; i < dbuf->buf_pointer && offset < sizeof(buf) - 10; i++) {
        if (i % 4 == 0) {
            offset += snprintf(buf + offset, sizeof(buf) - offset, " %02x", dbuf->buffer[i]);
        } else {
            offset += snprintf(buf + offset, sizeof(buf) - offset, "%02x", dbuf->buffer[i]);
        }
    }

    // Print entire message once
    ESP_LOGD (TAG, "%s", buf);
    }   

bool HPEmulator::check_checksum(struct DataBuffer* dbuf) {
    int i;
    uint8_t packetCheckSum = dbuf->buffer[dbuf->buf_pointer - 1];
    uint8_t processedCS = 0;
    for (i = 0; i < dbuf->length - 1; i++) {
        processedCS += dbuf->buffer[i];
    }
    processedCS = (0xfc - processedCS) & 0xff;
    return (packetCheckSum == processedCS);
}

void HPEmulator::add_checksum_to_packet(struct DataBuffer* dbuf) {
    int i;
    uint8_t processedCS = 0;
    for (i = 0; i < dbuf->length - 1; i++) {
        processedCS += dbuf->buffer[i];
    }
    dbuf->buffer[dbuf->length - 1] = (0xfc - processedCS) & 0xff;
}

const char* HPEmulator::lookupByteMapValue(const char* const valuesMap[], const uint8_t byteMap[], int len, uint8_t byteValue) {
    for (int i = 0; i < len; i++) {
        if (byteMap[i] == byteValue) {
            return valuesMap[i];
        }
    }
    return valuesMap[0];
}

int HPEmulator::lookupByteMapValue(const int valuesMap[], const uint8_t byteMap[], int len, uint8_t byteValue) {
    for (int i = 0; i < len; i++) {
        if (byteMap[i] == byteValue) {
            return valuesMap[i];
        }
    }
    return valuesMap[0];
}

int HPEmulator::lookupByteMapIndex(const char* valuesMap[], int len, const char* lookupValue) {
  for (int i = 0; i < len; i++) {
    if (strcasecmp(valuesMap[i], lookupValue) == 0) {
      return i;
    }
  }
  return -1;
}

void HPEmulator::send_ping_response_to_remote(struct DataBuffer* dbuf, uart_port_t uart_num) {
    Stim_buffer.buf_pointer = sizeof(PING_RESPONSE);
    Stim_buffer.length = Stim_buffer.buf_pointer;
    memcpy(Stim_buffer.buffer, PING_RESPONSE, sizeof(PING_RESPONSE));
    add_checksum_to_packet(&Stim_buffer);
    print_packet(&Stim_buffer, "Packet to", "RE");
    uart_write_bytes(uart_num, (const char*)Stim_buffer.buffer, Stim_buffer.buf_pointer);
}

void HPEmulator::send_config_response_to_remote(struct DataBuffer* dbuf, uart_port_t uart_num) {
    Stim_buffer.buf_pointer = sizeof(CONFIG_RESPONSE);
    Stim_buffer.length = Stim_buffer.buf_pointer;
    memcpy(Stim_buffer.buffer, CONFIG_RESPONSE, sizeof(CONFIG_RESPONSE));
    add_checksum_to_packet(&Stim_buffer);
    print_packet(&Stim_buffer, "Packet to", "RE");
    uart_write_bytes(uart_num, (const char*)Stim_buffer.buffer, Stim_buffer.buf_pointer);
}

void HPEmulator::send_remote_state_to_heatpump(struct DataBuffer* dbuf, uart_port_t uart_num) {
    //received a 0x41
           
    uint8_t mask1 = dbuf->buffer[6];
    uint8_t mask2 = dbuf->buffer[7];
        
    debugHPState("Emulator State before 0x41", emulatorState);

    if (mask1 & 0x01) setPower(&remoteState, dbuf->buffer[8]);
    if (mask1 & 0x02) setMode(&remoteState, dbuf->buffer[9]);
    if (mask1 & 0x04) {
        uint8_t temp = (dbuf->buffer[19] & 0x7f) >> 1;
        setTargetTemp(&remoteState, temp);
        setActualTemp(&remoteState, temp - 2); // For simplicity, set actual temp to target temp minus 2 degrees
        }
    if (mask1 & 0x08) setFanSpeed(&remoteState, dbuf->buffer[11]);
    if (mask1 & 0x10) setVaneVertical(&remoteState, dbuf->buffer[12]);
    if (mask2 & 0x01) setVaneHorizontal(&remoteState, dbuf->buffer[18]);

    //now create the data to send to esphome if a change happened
    debugHPState("Remote State after 0x41", remoteState);
               
    // send the response packet
    Stim_buffer.buf_pointer = sizeof(CONTROL_RESPONSE);
    Stim_buffer.length = Stim_buffer.buf_pointer;
    memcpy(Stim_buffer.buffer, CONTROL_RESPONSE, sizeof(CONTROL_RESPONSE));
    add_checksum_to_packet(&Stim_buffer);
    print_packet(&Stim_buffer, "Packet to", "RE");
    uart_write_bytes(uart_num, (const char*)Stim_buffer.buffer, Stim_buffer.buf_pointer);
}

void HPEmulator::send_heatpump_state_to_remote(struct DataBuffer* dbuf, uart_port_t uart_num) {
    //received a 0x62
    Stim_buffer.buf_pointer = sizeof(INFO_RESPONSE);
    Stim_buffer.length = Stim_buffer.buf_pointer;
    memcpy(Stim_buffer.buffer, INFO_RESPONSE, sizeof(INFO_RESPONSE));
    uint8_t info_mode = dbuf->buffer[5];
    Stim_buffer.buffer[5] = info_mode;
    switch(info_mode) {
        case 0x02: {
            //settings request
            Stim_buffer.buffer[8] = emulatorState.power;
            Stim_buffer.buffer[9] = emulatorState.mode;
            Stim_buffer.buffer[10] = emulatorState.setTemp;
            Stim_buffer.buffer[11] = emulatorState.fan;
            Stim_buffer.buffer[12] = emulatorState.vertVane;
            Stim_buffer.buffer[14] = emulatorState.horiVane;
            Stim_buffer.buffer[16] = (emulatorState.setTemp << 1) | 0x80; //target temp in bits 1-7
            break;
        }
        case 0x03: {
            // room temp request
            Stim_buffer.buffer[11] = (emulatorState.actualTemp << 1) | 0x80; //target temp in bits 1-7
            break;
        }
        case 0x04: {
            //unknown request
            Stim_buffer.buffer[9] = 0x80;
            break;
        }
        case 0x05: {
            //timer request
            break;
        }
        case 0x06: {
            //status request
            break;
        }
        case 0x09: {
            //standby mode request
            Stim_buffer.buffer[9] = 0x01;
            break;
        }
    }
    
    debugHPState("Emulator State sent in 0x62", emulatorState);
    add_checksum_to_packet(&Stim_buffer);
    print_packet(&Stim_buffer, "Packet to", "RE");
    uart_write_bytes(uart_num, (const char*)Stim_buffer.buffer, Stim_buffer.buf_pointer);

    // address the System UP.   Assume that it is 15 seconds after engineUP
    const uint64_t comparisonInterval = 15000; // 15 seconds in milliseconds
    uint64_t currentTime = esp_timer_get_time() / 1000; // Convert microseconds to milliseconds
    if ((currentTime - engineUpTime) >= comparisonInterval) {
        if (!systemUP) {
            systemUP = true;
            ESP_LOGD(TAG, "System is UP.");
            }
        }
    }

void HPEmulator::process_packets(struct DataBuffer* dbuf, uart_port_t uart_num) {
    //print the incoming packet
    print_packet(dbuf, "Packet to", "HP");

    uint8_t cmd = dbuf->buffer[1];

    if (cmd == 0x5a) this->send_ping_response_to_remote(dbuf, uart_num);
    else if (cmd == 0x5b) this->send_config_response_to_remote(dbuf, uart_num);
    else if (cmd == 0x41) {
        if (!(dbuf->buffer[4] == 0x10 && dbuf->buffer[5] == 0xa7 && dbuf->buffer[6] == 0x34 && dbuf->buffer[7] == 0x82))
            this->send_remote_state_to_heatpump(dbuf, uart_num);
    }
    else if (cmd == 0x42) this->send_heatpump_state_to_remote(dbuf, uart_num);
}

void HPEmulator::check_header(struct DataBuffer* dbuf) {
    if (dbuf->buf_pointer == 5) {
        if (dbuf->buffer[2] == HEADER[2] && dbuf->buffer[3] == HEADER[3]) {
            dbuf->command = dbuf->buffer[1];
            dbuf->length = dbuf->buffer[4] + 6; //length byte + header size + checksum
        }
    }
}

void HPEmulator::process_port_emulator(struct DataBuffer* dbuf, uart_port_t uart_num) {
    uint8_t data[256];
    int len = uart_read_bytes(uart_num, data, sizeof(data), 0);

    for (int i = 0; i < len; i++) {
        uint8_t S1byte = data[i];
        if (!dbuf->foundStart) {
            if (S1byte == HEADER[0]) { //start of packet
                dbuf->foundStart = true;
                dbuf->length = 22; //default length until we parse the real length
                dbuf->buffer[dbuf->buf_pointer++] = S1byte;
            }
        }
        else { //building a packet
            dbuf->buffer[dbuf->buf_pointer++] = S1byte;
            check_header(dbuf); //assign length and command if we have enough data
            if (dbuf->buf_pointer >= dbuf->length) {
                if (check_checksum(dbuf)) {
                    //valid packet
                    process_packets(dbuf, uart_num);
                }
                else print_packet(dbuf, "BAD Packet to", "HP");
                dbuf->foundStart = false;
                dbuf->buf_pointer = 0;
            }
        }
    }
}


bool HPEmulator::uartInit() {
    if (g_re_uart == nullptr) {
        ESP_LOGE(TAG, "uartInit: g_re_uart not set");
        return false;
    }
    uart_port_t port = (uart_port_t)g_re_uart->get_hw_serial_number();
    ESP_LOGD(TAG, "UART initialized by ESPHome (port %d)", (int)port);
    return true;
}

// Helper function to check if network is connected
static bool is_network_connected() {
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif == NULL) {
        return false;
    }

    esp_netif_ip_info_t ip_info;
    if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
        // Check if we have a valid IP address (not 0.0.0.0)
        return (ip_info.ip.addr != 0);
    }
    return false;
}

// --- Web Server Implementation ---

// Static web server handle
static httpd_handle_t web_server = NULL;


// HTTP server handler for heatpump status
esp_err_t heatpump_status_handler(httpd_req_t *req) {
    // Get HPEmulator instance from user context
    HPEmulator* hp = (HPEmulator*)req->user_ctx;

    // Use static buffer to avoid stack overflow
    static char html[8192];

    // Build HTML response
    int len = snprintf(html, sizeof(html), R"(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <meta http-equiv="refresh" content="10">
    <title>HP Emulator</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            margin: 10px;
            background-color: #f5f5f5;
            font-size: 13px;
        }
        .container {
            background-color: white;
            padding: 15px;
            border-radius: 6px;
            box-shadow: 0 2px 4px rgba(0,0,0,0.1);
            max-width: 900px;
            margin: 0 auto;
        }
        h1 {
            color: #333;
            text-align: center;
            margin: 0 0 15px 0;
            font-size: 20px;
        }
        .grid {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 10px;
        }
        .status-item {
            background-color: #f9f9f9;
            padding: 10px;
            border-left: 3px solid #4CAF50;
            border-radius: 3px;
        }
        .status-item.mismatch {
            border-left-color: #ff9800;
            background-color: #fff3e0;
        }
        .status-label {
            font-weight: bold;
            color: #555;
            font-size: 11px;
            text-transform: uppercase;
            margin-bottom: 6px;
        }
        .values-container {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 8px;
        }
        .value-box {
            text-align: center;
        }
        .value-type {
            font-size: 9px;
            color: #888;
            margin-bottom: 3px;
        }
        .status-value {
            font-size: 16px;
            color: #4CAF50;
            font-weight: bold;
        }
        .footer {
            text-align: center;
            margin-top: 10px;
            font-size: 10px;
            color: #999;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>Heatpump Emulator</h1>
        <div class="grid">
            <div class="status-item%s">
                <div class="status-label">Power</div>
                <div class="values-container">
                    <div class="value-box">
                        <div class="value-type">Emulator</div>
                        <div class="status-value">%s</div>
                    </div>
                    <div class="value-box">
                        <div class="value-type">Esphome</div>
                        <div class="status-value">%s</div>
                    </div>
                </div>
            </div>

            <div class="status-item%s">
                <div class="status-label">Mode</div>
                <div class="values-container">
                    <div class="value-box">
                        <div class="value-type">Emulator</div>
                        <div class="status-value">%s</div>
                    </div>
                    <div class="value-box">
                        <div class="value-type">Esphome</div>
                        <div class="status-value">%s</div>
                    </div>
                </div>
            </div>

            <div class="status-item%s">
                <div class="status-label">Fan Speed</div>
                <div class="values-container">
                    <div class="value-box">
                        <div class="value-type">Emulator</div>
                        <div class="status-value">%s</div>
                    </div>
                    <div class="value-box">
                        <div class="value-type">Esphome</div>
                        <div class="status-value">%s</div>
                    </div>
                </div>
            </div>

            <div style="visibility: hidden;"></div>

            <div class="status-item%s">
                <div class="status-label">Target Temp (°C)</div>
                <div class="values-container">
                    <div class="value-box">
                        <div class="value-type">Emulator</div>
                        <div class="status-value">%d</div>
                    </div>
                    <div class="value-box">
                        <div class="value-type">Esphome</div>
                        <div class="status-value">%d</div>
                    </div>
                </div>
            </div>

            <div class="status-item%s">
                <div class="status-label">Target Temp (°F)</div>
                <div class="values-container">
                    <div class="value-box">
                        <div class="value-type">Emulator</div>
                        <div class="status-value">%d</div>
                    </div>
                    <div class="value-box">
                        <div class="value-type">Esphome</div>
                        <div class="status-value">%d</div>
                    </div>
                </div>
            </div>

            <div class="status-item%s">
                <div class="status-label">Actual Temp (°C)</div>
                <div class="values-container">
                    <div class="value-box">
                        <div class="value-type">Emulator</div>
                        <div class="status-value">%d</div>
                    </div>
                    <div class="value-box">
                        <div class="value-type">Esphome</div>
                        <div class="status-value">%d</div>
                    </div>
                </div>
            </div>

            <div class="status-item%s">
                <div class="status-label">Actual Temp (°F)</div>
                <div class="values-container">
                    <div class="value-box">
                        <div class="value-type">Emulator</div>
                        <div class="status-value">%d</div>
                    </div>
                    <div class="value-box">
                        <div class="value-type">Esphome</div>
                        <div class="status-value">%d</div>
                    </div>
                </div>
            </div>

            <div class="status-item%s">
                <div class="status-label">Vane Vertical</div>
                <div class="values-container">
                    <div class="value-box">
                        <div class="value-type">Emulator</div>
                        <div class="status-value">%s</div>
                    </div>
                    <div class="value-box">
                        <div class="value-type">Esphome</div>
                        <div class="status-value">%s</div>
                    </div>
                </div>
            </div>

            <div class="status-item%s">
                <div class="status-label">Vane Horizontal</div>
                <div class="values-container">
                    <div class="value-box">
                        <div class="value-type">Emulator</div>
                        <div class="status-value">%s</div>
                    </div>
                    <div class="value-box">
                        <div class="value-type">Esphome</div>
                        <div class="status-value">%s</div>
                    </div>
                </div>
            </div>
        </div>
        <div class="footer">
            Auto-refresh: 10s | Heatpump Emulator
        </div>
    </div>
</body>
</html>
)",
        "",
        hp->lookupByteMapValue(hp->POWER_MAP, hp->POWER, 2, hp->emulatorState.power),
        hp->lookupByteMapValue(hp->POWER_MAP, hp->POWER, 2, hp->esphomeState.power),

        "",
        hp->lookupByteMapValue(hp->MODE_MAP, hp->MODE, 5, hp->emulatorState.mode),
        hp->lookupByteMapValue(hp->MODE_MAP, hp->MODE, 5, hp->esphomeState.mode),

        "",
        hp->lookupByteMapValue(hp->FAN_MAP, hp->FAN, 6, hp->emulatorState.fan),
        hp->lookupByteMapValue(hp->FAN_MAP, hp->FAN, 6, hp->esphomeState.fan),

        "",
        hp->emulatorState.setTemp,
        hp->esphomeState.setTemp,

        "",
        (hp->emulatorState.setTemp * 9 / 5) + 32,
        (hp->esphomeState.setTemp * 9 / 5) + 32,

        "",
        hp->emulatorState.actualTemp,
        hp->esphomeState.actualTemp,

        "",
        (hp->emulatorState.actualTemp * 9 / 5) + 32,
        (hp->esphomeState.actualTemp * 9 / 5) + 32,

        "",
        hp->lookupByteMapValue(hp->VANE_MAP, hp->VANE, 7, hp->emulatorState.vertVane),
        hp->lookupByteMapValue(hp->VANE_MAP, hp->VANE, 7, hp->esphomeState.vertVane),

        "",
        hp->lookupByteMapValue(hp->WIDEVANE_MAP, hp->WIDEVANE, 7, hp->emulatorState.horiVane),
        hp->lookupByteMapValue(hp->WIDEVANE_MAP, hp->WIDEVANE, 7, hp->esphomeState.horiVane)
    );

    if (len < 0 || len >= sizeof(html)) {
        ESP_LOGE(TAG, "HTML buffer overflow!");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
    httpd_resp_set_hdr(req, "Pragma", "no-cache");
    httpd_resp_set_hdr(req, "Expires", "0");
    httpd_resp_send(req, html, len);
    return ESP_OK;
}

static esp_err_t not_found_handler(httpd_req_t *req, httpd_err_code_t err) {
    httpd_resp_send_404(req);
    return ESP_OK;
}

// Start web server
void* HPEmulator::start_webserver() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;
    config.stack_size = 8192;  // Increase stack size for HTTP handler
    config.server_port = WEBPORT;
    config.ctrl_port = 32769; // Avoid conflict with main ESPHome server

    ESP_LOGD(TAG, "Starting web server on port %d", config.server_port);

    if (httpd_start(&web_server, &config) == ESP_OK) {
        // Register URI handlers - pass 'this' as user context
        httpd_uri_t uri_status = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = heatpump_status_handler,
            .user_ctx = this
        };
        httpd_register_uri_handler(web_server, &uri_status);

        // Register 404 handler
        httpd_register_err_handler(web_server, HTTPD_404_NOT_FOUND, not_found_handler);

        return web_server;
    }

    ESP_LOGE(TAG, "Error starting web server!");
    return NULL;
}

void HPEmulator::setup() {
    ESP_LOGD(TAG, "Starting HPEmulator setup");
    //if (!uartInit()) ESP_LOGE(TAG, "Failed to initialize UART");
    
    //initialize some variables
    _webserver_started=false;
    systemUP=false;
    engineUpTime=0;
    
    ESP_LOGD(TAG, "HPEmulator setup complete (webserver will start when network is ready)");
}

void HPEmulator::run() {
    //read the serial port and update the emulator state
    if (g_re_uart == nullptr) return;
    process_port_emulator(&Remote_buffer, (uart_port_t)g_re_uart->get_hw_serial_number());

    // Start webserver once network is available
    if (!_webserver_started && is_network_connected()) {
        if (start_webserver()) {
            ESP_LOGD(TAG, "Web server started on port %d", WEBPORT);
            _webserver_started = true;
        } else {
            ESP_LOGE(TAG, "Failed to start web server");
        }
    }
    
    //look for any change frome the remote interface without delay
    checkForRemoteStateChange();

    // Get ESPHome state every second
    static uint64_t lastComparisonTime = 0;
    const uint64_t comparisonInterval = 1000; // 1 second in milliseconds
    uint64_t currentTime = esp_timer_get_time() / 1000; // Convert microseconds to milliseconds
    if ((currentTime - lastComparisonTime) >= comparisonInterval) {
        updateEmulatorStateFromEngine();
        //simpleOperation(); //used for testing only
        lastComparisonTime = currentTime;
        }
}

} // namespace HVAC