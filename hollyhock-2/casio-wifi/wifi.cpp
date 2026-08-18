#include "wifi.h"
#include "rtc.h"
#include <sdk/calc/calc.hpp>
#include <sdk/os/serial.hpp>
#include <sdk/os/string.hpp>
#include <sdk/os/lcd.hpp>
#include <sdk/os/debug.hpp>
#include <sdk/os/mem.hpp>

static wifi_scan_list_t * current_scan_list = nullptr;

static char * strncpy(char *dest, const char *src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i] != '\0'; i++)
        dest[i] = src[i];
    for (; i < n; i++)
        dest[i] = '\0';
    return dest;
}

static uint16_t swap_uint16(uint16_t val) {
    return (val << 8) | (val >> 8);
}

static uint8_t calculate_checksum(msg_header_t* header, uint8_t* payload) {
    uint8_t checksum = 0;
    checksum ^= header->command_id; // Command Id
    uint8_t* len_ptr = (uint8_t*)&header->payload_len; // Payload Len
    checksum ^= len_ptr[0];
    checksum ^= len_ptr[1];

    if (payload != nullptr && header->payload_len > 0) {
        for (uint16_t i = 0; i < header->payload_len; i++) {
            checksum ^= payload[i]; // Payload Bytes
        }
    }

    return checksum;
}

static int send_buffer(const unsigned char* buffer, size_t length) {
    size_t offset = 0;
    while (offset < length) {
        size_t chunk_size = (length - offset) > WIFI_SERIAL_BUFFER_SIZE ? WIFI_SERIAL_BUFFER_SIZE : (length - offset);
        
        int res = Serial_Write(buffer + offset, chunk_size);
        if (res != 0) return res;
        
        offset += chunk_size;

        rtc_sleep_ms(1); 
    }
    return 0;
}

static int serial_read_exact(unsigned char* target, size_t total_len, uint32_t timeout_ms) {
    size_t received = 0;
    short count = 0;

    while (received < total_len && timeout_ms > 0) {
        size_t to_read = (total_len - received);
        if (Serial_Read(target + received, (short)to_read, &count) == 0 && count > 0) {
            received += count;
        } else {
            rtc_sleep_ms(1);
            timeout_ms--;
        }
    }
    return (received == total_len) ? 0 : -1;
}

static int wifi_receive_packet(uint8_t expected_cmd, uint8_t** out_payload, uint16_t* out_len) {
    unsigned char c;
    short count;
    uint32_t retries = 2000;

    while (retries-- > 0) {
        if (Serial_Read(&c, 1, &count) == 0 && count > 0 && c == STX) {
            msg_header_t header;
            header.start_byte = c;

            if (serial_read_exact(((unsigned char*)&header) + 1, sizeof(msg_header_t) - 1, 100) != 0) return -1;

            uint16_t real_len = header.payload_len;
            
            uint8_t* payload = nullptr;
            if (real_len > 0) {
                payload = (uint8_t*)malloc(real_len);
                if (!payload) return -2;

                if (serial_read_exact(payload, real_len, 500) != 0) {
                    free(payload);
                    return -3;
                }
            }

            msg_footer_t footer;
            if (serial_read_exact((unsigned char*)&footer, sizeof(msg_footer_t), 100) != 0) {
                if (payload) free(payload);
                return -4;
            }

            if (footer.checksum == calculate_checksum(&header, payload) && footer.end_byte == ETX && header.command_id == expected_cmd) {
                *out_payload = payload;
                *out_len = real_len;
                return 0; // OK
            }

            if (payload) free(payload);
            return -5; // Checksummenfehler
        }
        rtc_sleep_ms(1);
    }
    return -6; // Timeout
}

static void send_ack() {
    msg_header_t header = { STX, CMD_ACK, 0x0000 }; // payload_len 0
    uint8_t cs = calculate_checksum(&header, nullptr);
    msg_footer_t footer = { cs, ETX };
    
    send_buffer((unsigned char*)&header, sizeof(header));
    send_buffer((unsigned char*)&footer, sizeof(footer));
}

static int wait_for_ack() {
    uint8_t* payload = nullptr;
    uint16_t payload_len = 0;
    int res = wifi_receive_packet(CMD_RESPONSE_OK, &payload, &payload_len);
    if (res == 0) {
        if (payload) free(payload);
        return 0; // OK
    } else {
        return -1; // Error or Timeout
    }
}

static int wifi_receive_transceive_packet(uint8_t expected_cmd, uint8_t** out_data, uint16_t* out_len) {
    uint8_t* full_buffer = nullptr;
    uint32_t total_received_bytes = 0;
    uint16_t current_chunk_idx = 0;
    uint16_t total_chunks_to_wait = 1;

    while (current_chunk_idx < total_chunks_to_wait) {
        unsigned char c;
        short count;
        uint32_t retries = 5000;

        bool found_stx = false;
        while (retries-- > 0) {
            if (Serial_Read(&c, 1, &count) == 0 && count > 0 && c == STX) {
                found_stx = true;
                Debug_Printf(0, 15, true, 0, "Received STX for chunk %d", current_chunk_idx);
                LCD_Refresh();
                break;
            }
            rtc_sleep_ms(1);
        }
        if (!found_stx) return -6; // Timeout

        msg_header_t header;
        header.start_byte = STX;
        if (serial_read_exact(((unsigned char*)&header) + 1, sizeof(msg_header_t) - 1, 200) != 0) return -1;

        uint16_t payload_len = header.payload_len;
        Debug_Printf(0, 16, true, 0, "Chunk %d: Payload Length: %d", current_chunk_idx, payload_len);
        LCD_Refresh();
        
        uint8_t* chunk_payload_raw = (uint8_t*)malloc(payload_len);
        if (!chunk_payload_raw) return -2;

        if (serial_read_exact(chunk_payload_raw, payload_len, 500) != 0) {
            free(chunk_payload_raw);
            return -3;
        }

        msg_footer_t footer;
        if (serial_read_exact((unsigned char*)&footer, sizeof(msg_footer_t), 200) != 0) {
            free(chunk_payload_raw);
            return -4;
        }

        if (footer.checksum != calculate_checksum(&header, chunk_payload_raw) || footer.end_byte != ETX) {
            free(chunk_payload_raw);
            return -5; // CS Error
        }

        if (expected_cmd != header.command_id) {
            free(chunk_payload_raw);
            return -7; // Error Unknown Command
        }

        wifi_transceive_response_t* chunk = (wifi_transceive_response_t*)chunk_payload_raw;
        uint16_t n_total_chunks = chunk->total_chunks;
        uint16_t n_current_chunk = chunk->current_chunk;
        uint16_t n_data_len = chunk->data_len;

        if (n_current_chunk == 0) {
            total_chunks_to_wait = n_total_chunks;
            full_buffer = (uint8_t*)malloc(n_total_chunks * CHUNK_SIZE + 1); 
            if (!full_buffer) { free(chunk_payload_raw); return -2; }
        }

        if (n_current_chunk >= total_chunks_to_wait) {
            free(chunk_payload_raw);
            return -8; // Error Invalid Chunk Index
        }

        if (total_received_bytes + n_data_len > (n_total_chunks * CHUNK_SIZE)) {
            free(chunk_payload_raw);
            return -9; // Error Data Overflow
        }

        memcpy(full_buffer + total_received_bytes, chunk->data, n_data_len);
        total_received_bytes += n_data_len;
        current_chunk_idx++;

        free(chunk_payload_raw);
        send_ack();
    }

    full_buffer[total_received_bytes] = '\0';
    *out_data = full_buffer;
    *out_len = (uint16_t)total_received_bytes;
    return 0;
}

int wifi_init() {
    if (Serial_IsOpen() != 1) {
        static unsigned char mode[] = WIFI_SERIAL_MODE;
        int res = Serial_Open(mode);
        if (res != 0) return res;
        return 0;
    }
    return -1;
}

int wifi_deinit() {
    wifi_disconnect();
    if (Serial_IsOpen() == 1) {
        Serial_Close(1);
    }
    wifi_free_scan_list();
    return 0;
}

int wifi_connect(const char * ssid, const char * password) {
    Serial_ClearRX();
    Serial_ClearTX();

    msg_header_t header;
    header.start_byte = STX;
    header.command_id = CMD_WIFI_CONNECT;
    header.payload_len = sizeof(wifi_connect_payload_t);

    wifi_connect_payload_t payload;
    memset(&payload, 0, sizeof(payload));
    strncpy(payload.ssid, ssid, 32);
    strncpy(payload.password, password, 64);

    uint8_t cs = calculate_checksum(&header, (uint8_t*)&payload);
    
    header.payload_len = swap_uint16(header.payload_len); // Convert to big-endian for transmission to esp32

    msg_footer_t footer = { cs, ETX };

    if (send_buffer((unsigned char*)&header, sizeof(header)) != 0) return -3;
    if (send_buffer((unsigned char*)&payload, sizeof(payload)) != 0) return -3;
    if (send_buffer((unsigned char*)&footer, sizeof(footer)) != 0) return -3;

    return wait_for_ack();
}

int wifi_disconnect() {
    Serial_ClearRX();
    Serial_ClearTX();

    msg_header_t header;
    header.start_byte = STX;
    header.command_id = CMD_WIFI_DISCONNECT;
    header.payload_len = 0;

    uint8_t cs = calculate_checksum(&header, (uint8_t*)nullptr);

    msg_footer_t footer = { cs, ETX };

    if (send_buffer((unsigned char*)&header, sizeof(header)) != 0) return -3;
    if (send_buffer((unsigned char*)&footer, sizeof(footer)) != 0) return -3;

    return wait_for_ack();
}

uint8_t wifi_status() {
    Serial_ClearRX();
    Serial_ClearTX();

    msg_header_t header;
    header.start_byte = STX;
    header.command_id = CMD_WIFI_STATUS;
    header.payload_len = 0;

    uint8_t cs = calculate_checksum(&header, nullptr);
    msg_footer_t footer = { cs, ETX };

    if (send_buffer((unsigned char*)&header, sizeof(header)) != 0) return WIFI_STATUS_ERROR;
    if (send_buffer((unsigned char*)&footer, sizeof(footer)) != 0) return WIFI_STATUS_ERROR;

    uint8_t* payload = nullptr;
    uint16_t payload_len = 0;
    int res = wifi_receive_packet(CMD_RESPONSE_OK, &payload, &payload_len);

    if (res == 0 && payload && payload_len >= sizeof(wifi_status_response_t)) {
        wifi_status_response_t* status_resp = (wifi_status_response_t*)payload;
        uint8_t status = status_resp->status;
        free(payload);
        return status;
    }

    if (payload) free(payload);
    return WIFI_STATUS_ERROR;
}

int wifi_get_ip(char * buffer, uint32_t buffer_size) {
    if (!buffer || buffer_size < 16) return -1;

    Serial_ClearRX();
    Serial_ClearTX();

    msg_header_t header;
    header.start_byte = STX;
    header.command_id = CMD_WIFI_GET_IP;
    header.payload_len = 0;

    uint8_t cs = calculate_checksum(&header, nullptr);
    msg_footer_t footer = { cs, ETX };

    if (send_buffer((unsigned char*)&header, sizeof(header)) != 0) return -3;
    if (send_buffer((unsigned char*)&footer, sizeof(footer)) != 0) return -3;

    uint8_t* payload = nullptr;
    uint16_t payload_len = 0;
    int res = wifi_receive_packet(CMD_RESPONSE_OK, &payload, &payload_len);

    if (res == 0 && payload && payload_len >= sizeof(wifi_get_ip_response_t)) {
        wifi_get_ip_response_t* ip_resp = (wifi_get_ip_response_t*)payload;
        strncpy(buffer, ip_resp->ip_address, buffer_size);
        free(payload);
        return 0;
    }

    if (payload) free(payload);
    return -4;
}

wifi_scan_list_t * wifi_scan() {
    Serial_ClearRX();
    Serial_ClearTX();

    msg_header_t header;
    header.start_byte = STX;
    header.command_id = CMD_WIFI_SCAN;
    header.payload_len = 0;

    uint8_t cs = calculate_checksum(&header, nullptr);
    msg_footer_t footer = { cs, ETX };

    if (send_buffer((unsigned char*)&header, sizeof(header)) != 0) return nullptr;
    if (send_buffer((unsigned char*)&footer, sizeof(footer)) != 0) return nullptr;

    uint8_t* payload = nullptr;
    uint16_t payload_len = 0;
    int res = wifi_receive_packet(CMD_RESPONSE_OK, &payload, &payload_len);

    if (res == 0 && payload && payload_len >= sizeof(uint16_t)) {
        wifi_free_scan_list();
        current_scan_list = (wifi_scan_list_t*)payload;
        return current_scan_list;
    }

    if (payload) free(payload);
    return nullptr;
}

void wifi_free_scan_list() {
    if (current_scan_list) {
        free(current_scan_list);
        current_scan_list = nullptr;
    }
}

uint8_t * wifi_transceive(const char * request, uint16_t * response_size) {
    Serial_ClearRX();
    Serial_ClearTX();

    uint16_t req_data_len = (uint16_t)strlen(request);
    uint16_t total_payload_len = sizeof(uint16_t) + req_data_len;

    msg_header_t header;
    header.start_byte = STX;
    header.command_id = CMD_TRANSCEIVE;
    header.payload_len = total_payload_len;

    wifi_transceive_payload_t *payload = (wifi_transceive_payload_t*)malloc(total_payload_len);
    if (!payload) return nullptr;

    memcpy(payload->data, request, req_data_len);
    payload->data_len = swap_uint16(req_data_len);

    uint8_t cs = calculate_checksum(&header, (uint8_t*)payload);
    
    header.payload_len = swap_uint16(total_payload_len); // Convert to big-endian for transmission to esp32

    msg_footer_t footer = { cs, ETX };

    if (send_buffer((unsigned char*)&header, sizeof(header)) != 0 ||
        send_buffer((unsigned char*)payload, total_payload_len) != 0 ||
        send_buffer((unsigned char*)&footer, sizeof(footer)) != 0) {
        free(payload);
        return nullptr;
    }

    free(payload);

    uint8_t* resp_data = nullptr;
    uint16_t resp_len = 0;
    int res = wifi_receive_transceive_packet(CMD_RESPONSE_OK, &resp_data, &resp_len);

    Debug_Printf(0, 30, true, 0, "Transceive receive result: %d, payload_len: %d", res, resp_len);
    LCD_Refresh();

    if (res == 0 && resp_data) {
        Debug_Printf(0, 31, true, 0, "Total Data Length: %04X", resp_len);
        LCD_Refresh();

        if (response_size) *response_size = resp_len;
        return resp_data; // needs to be clear after use
    }

    if (resp_data) free(resp_data);
    return nullptr;
}

void wifi_free_transceive_response(uint8_t * response) {
    if (response) {
        free(response);
    }
}