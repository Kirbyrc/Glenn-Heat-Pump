#include <stdio.h>
#include <string.h>
#include "hp_emulator_idf.h"
#include "cn105.h"
#include "driver/uart.h"
#include "esphome.h"
#include "esp_timer.h"
#include "esp_http_server.h"
#include "esp_netif.h"

// Global pointer to CN105Climate instance
esphome::CN105Climate* g_cn105 = nullptr;

namespace HVAC {

static const char *TAG = "HPE_Core";

// Constructor initializes variables with default values
HPEmulator::HPEmulator() :
    emulatorPower(1), emulatorMode(2), emulatorFan(3), emulatorSetTemp(20), emulatorActualTemp(18),
    emulatorVertVane(3), emulatorHoriVane(1), _webserver_started(false) {
}

// --- Initialization Method ---
void HPEmulator::stateInit(uint8_t power, uint8_t mode, uint8_t fan_speed,
                      uint8_t set_temp, uint8_t act_temp, uint8_t vane_vertical,
                      uint8_t vane_horizontal) {
    emulatorPower = power;
    emulatorMode = mode;
    emulatorFan = fan_speed;
    emulatorSetTemp = set_temp;
    emulatorActualTemp = act_temp;
    emulatorVertVane = vane_vertical;
    emulatorHoriVane = vane_horizontal;
}

// --- Setters ---
void HPEmulator::setPower(uint8_t value) {
    if (value > 1) print_byte(value, "Power", "Out of Range");
    else emulatorPower = value;
}

void HPEmulator::setMode(uint8_t value) {
    if (value > 0x08) print_byte(value, "Mode", "Out of Range");
    else emulatorMode = value;
}

void HPEmulator::setFanSpeed(uint8_t value) {
    if (value > 6) print_byte(value, "Fan Speed", "Out of Range");
    else emulatorFan = value;
}

void HPEmulator::setTargetTemp(uint8_t value) {
    if (value < 0x10) print_byte(value, "Target Temp", "Out of Range");
    else emulatorSetTemp = value;
}

void HPEmulator::setActualTemp(uint8_t value) {
    if (value < 0x10) print_byte(value, "Actual Temp", "Out of Range");
    else emulatorActualTemp = value;
}

void HPEmulator::setVaneVertical(uint8_t value) {
    if (value > 7) print_byte(value, "Vane Vertical", "Out of Range");
    else emulatorVertVane = value;
}

void HPEmulator::setVaneHorizontal(uint8_t value) {
    if (value > 12) print_byte(value, "Vane Horizontal", "Out of Range");
    else emulatorHoriVane = value;
}


// --- Comparison ---
void HPEmulator::getEsphomeState() {
    int index;
    
    if (g_cn105 == nullptr) {
        ESP_LOGW(TAG, "g_cn105 is null, cannot get ESPHome state");
        return;
    }

    // Print currentSettings from CN105Climate (now public - KIRBY)
    ESP_LOGI(TAG, "ESPHome currentSettings:");
    ESP_LOGI(TAG, "  power: %s", g_cn105->currentSettings.power ? g_cn105->currentSettings.power : "null");
    ESP_LOGI(TAG, "  mode: %s", g_cn105->currentSettings.mode ? g_cn105->currentSettings.mode : "null");
    ESP_LOGI(TAG, "  temperature: %.1f", g_cn105->currentSettings.temperature);
    ESP_LOGI(TAG, "  fan: %s", g_cn105->currentSettings.fan ? g_cn105->currentSettings.fan : "null");
    ESP_LOGI(TAG, "  vane: %s", g_cn105->currentSettings.vane ? g_cn105->currentSettings.vane : "null");
    ESP_LOGI(TAG, "  wideVane: %s", g_cn105->currentSettings.wideVane ? g_cn105->currentSettings.wideVane : "null");

    // Temperatures
    esphomeSetTemp = (uint8_t)g_cn105->currentSettings.temperature;
    esphomeActualTemp = (uint8_t)g_cn105->current_temperature;
    
    // For the others: lookup byte value from string
    if (g_cn105->currentSettings.power) {
        index = lookupByteMapIndex(POWER_MAP, 2, g_cn105->currentSettings.power);
        if (index <0) esphomePower = 0;
        else esphomePower = POWER[index];
        }

    if (g_cn105->currentSettings.mode) {
        index = lookupByteMapIndex(MODE_MAP, 5, g_cn105->currentSettings.mode);
        if (index <0) esphomeMode = 0;
        else esphomeMode = MODE[index];
        }

    if (g_cn105->currentSettings.fan) {
        index = lookupByteMapIndex(FAN_MAP, 6, g_cn105->currentSettings.fan);
        if (index <0) esphomeFan = 0;
        else esphomeFan = FAN[index];
        }

    if (g_cn105->currentSettings.vane) {
        index = lookupByteMapIndex(VANE_MAP, 7, g_cn105->currentSettings.vane);
        if (index <0) esphomeVertVane = 0;
        else esphomeVertVane = VANE[index];
        }

    if (g_cn105->currentSettings.wideVane) {
        index = lookupByteMapIndex(WIDEVANE_MAP, 7, g_cn105->currentSettings.wideVane);
        if (index <0) esphomeHoriVane = 0;
        else esphomeHoriVane = WIDEVANE[index];
        }

     // Push esphome state to emulator
    emulatorPower = esphomePower;
    emulatorMode = esphomeMode;
    emulatorFan = esphomeFan;      
    emulatorSetTemp = esphomeSetTemp;
    emulatorActualTemp = esphomeActualTemp;
    emulatorVertVane = esphomeVertVane;
    emulatorHoriVane = esphomeHoriVane;
    }

bool HPEmulator::compareWithESPHOME() const {
    return (true);
    return (esphomePower == emulatorPower &&
            esphomeMode == emulatorMode &&
            esphomeFan == emulatorFan &&
            esphomeSetTemp == emulatorSetTemp &&
            esphomeVertVane == emulatorVertVane &&
            esphomeHoriVane == emulatorHoriVane);
   }

// variables
DataBuffer Stim_buffer; //used to build stimulus
DataBuffer Remote_buffer; //used to receive from remote

// --- Logic Methods ---

void HPEmulator::print_utility(const char* data) {
    //printf("%s", data);
    ESP_LOGI(TAG, "%s", data);
}

void HPEmulator::print_byte(uint8_t byte, const char* mess1, const char* mess2) {
    char buf[256];
    uint64_t currentMS = esp_timer_get_time() / 1000; // Convert microseconds to milliseconds
    snprintf(buf, 256, "(%08lld-000) %s %s 0x%02x\n", currentMS, mess1, mess2, byte);
    HPEmulator::print_utility(buf);
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
    print_utility(buf);
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
    uint8_t mask1 = dbuf->buffer[6];
    uint8_t mask2 = dbuf->buffer[7];

    if (mask1 & 0x01) {
        setPower(dbuf->buffer[8]);
       }
    if (mask1 & 0x02) {
        setMode(dbuf->buffer[9]);
       }
    if (mask1 & 0x04) {
        uint8_t temp = (dbuf->buffer[19] & 0x7f) >> 1;
        setTargetTemp(temp);
        setActualTemp(temp - 2); // For simplicity, set actual temp to target temp minus 2 degrees
       }
    if (mask1 & 0x08) {
        setFanSpeed(dbuf->buffer[11]);
       }
    if (mask1 & 0x10) {
        setVaneVertical(dbuf->buffer[12]);
       }
    if (mask2 & 0x01) {
        setVaneHorizontal(dbuf->buffer[18]);
       }

    Stim_buffer.buf_pointer = sizeof(CONTROL_RESPONSE);
    Stim_buffer.length = Stim_buffer.buf_pointer;
    memcpy(Stim_buffer.buffer, CONTROL_RESPONSE, sizeof(CONTROL_RESPONSE));
    add_checksum_to_packet(&Stim_buffer);
    print_packet(&Stim_buffer, "Packet to", "RE");
    uart_write_bytes(uart_num, (const char*)Stim_buffer.buffer, Stim_buffer.buf_pointer);
}

void HPEmulator::send_heatpump_state_to_remote(struct DataBuffer* dbuf, uart_port_t uart_num) {
    Stim_buffer.buf_pointer = sizeof(INFO_RESPONSE);
    Stim_buffer.length = Stim_buffer.buf_pointer;
    memcpy(Stim_buffer.buffer, INFO_RESPONSE, sizeof(INFO_RESPONSE));
    uint8_t info_mode = dbuf->buffer[5];
    Stim_buffer.buffer[5] = info_mode;

    switch(info_mode) {
        case 0x02: {
            //settings request
            Stim_buffer.buffer[8] = emulatorPower;
            Stim_buffer.buffer[9] = emulatorMode;
            Stim_buffer.buffer[10] = emulatorSetTemp;
            Stim_buffer.buffer[11] = emulatorFan;
            Stim_buffer.buffer[12] = emulatorVertVane;
            Stim_buffer.buffer[14] = emulatorHoriVane;
            Stim_buffer.buffer[16] = (emulatorSetTemp << 1) | 0x80; //target temp in bits 1-7
            break;
        }
        case 0x03: {
            // room temp request
            Stim_buffer.buffer[11] = (emulatorActualTemp << 1) | 0x80; //target temp in bits 1-7
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

    add_checksum_to_packet(&Stim_buffer);
    print_packet(&Stim_buffer, "Packet to", "RE");
    uart_write_bytes(uart_num, (const char*)Stim_buffer.buffer, Stim_buffer.buf_pointer);
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
    ESP_LOGI(TAG, "Initializing UART");

    // Configure UART parameters
    uart_config_t uart_config = {
        .baud_rate = BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_EVEN,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 122,
        .source_clk = UART_SCLK_APB,
        .flags = {},
    };

    // Install UART driver for RE_UART (Serial2)
    if (uart_driver_install(RE_UART_NUM, 256 * 2, 0, 0, NULL, 0) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install UART driver");
        return false;
    }

    if (uart_param_config(RE_UART_NUM, &uart_config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure UART parameters");
        return false;
    }

    if (uart_set_pin(RE_UART_NUM, RE_TX2_PIN, RE_RX2_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set UART pins");
        return false;
    }

    ESP_LOGI(TAG, "UART initialized: TX=%d RX=%d Baud=%d", RE_TX2_PIN, RE_RX2_PIN, BAUD);
    return true;
}

// --- Web Server Implementation ---

// Static web server handle
static httpd_handle_t web_server = NULL;

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
        hp->lookupByteMapValue(hp->POWER_MAP, hp->POWER, 2, hp->emulatorPower),
        hp->lookupByteMapValue(hp->POWER_MAP, hp->POWER, 2, hp->esphomePower),

        "",
        hp->lookupByteMapValue(hp->MODE_MAP, hp->MODE, 5, hp->emulatorMode),
        hp->lookupByteMapValue(hp->MODE_MAP, hp->MODE, 5, hp->esphomeMode),

        "",
        hp->lookupByteMapValue(hp->FAN_MAP, hp->FAN, 6, hp->emulatorFan),
        hp->lookupByteMapValue(hp->FAN_MAP, hp->FAN, 6, hp->esphomeFan),

        "",
        hp->emulatorSetTemp,
        hp->esphomeSetTemp,

        "",
        (hp->emulatorSetTemp * 9 / 5) + 32,
        (hp->esphomeSetTemp * 9 / 5) + 32,

        "",
        hp->emulatorActualTemp,
        hp->esphomeActualTemp,

        "",
        (hp->emulatorActualTemp * 9 / 5) + 32,
        (hp->esphomeActualTemp * 9 / 5) + 32,

        "",
        hp->lookupByteMapValue(hp->VANE_MAP, hp->VANE, 7, hp->emulatorVertVane),
        hp->lookupByteMapValue(hp->VANE_MAP, hp->VANE, 7, hp->esphomeVertVane),

        "",
        hp->lookupByteMapValue(hp->WIDEVANE_MAP, hp->WIDEVANE, 7, hp->emulatorHoriVane),
        hp->lookupByteMapValue(hp->WIDEVANE_MAP, hp->WIDEVANE, 7, hp->esphomeHoriVane)
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

    ESP_LOGI(TAG, "Starting web server on port %d", config.server_port);

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
    ESP_LOGI(TAG, "Starting HPEmulator setup");
    if (uartInit()) ESP_LOGI(TAG, "UART initialized successfully");
    else ESP_LOGE(TAG, "Failed to initialize UART");

    ESP_LOGI(TAG, "HPEmulator setup complete (webserver will start when network is ready)");
}

void HPEmulator::run() {
    process_port_emulator(&Remote_buffer, RE_UART_NUM);

    // Start webserver once network is available
    if (!_webserver_started && is_network_connected()) {
        if (start_webserver()) {
            ESP_LOGI(TAG, "Web server started on port %d", WEBPORT);
            _webserver_started = true;
        } else {
            ESP_LOGE(TAG, "Failed to start web server");
        }
    }

    // Compare global variables with internal state every second
    static uint64_t lastComparisonTime = 0;
    const uint64_t comparisonInterval = 1000; // 1 second in milliseconds

    uint64_t currentTime = esp_timer_get_time() / 1000; // Convert microseconds to milliseconds
    if ((currentTime - lastComparisonTime) >= comparisonInterval) {
        getEsphomeState();
        bool match = compareWithESPHOME();
        if (!match) {
            ESP_LOGW(TAG, "Global variables differ from internal state");
            }
        lastComparisonTime = currentTime;
    }
}

} // namespace HVAC