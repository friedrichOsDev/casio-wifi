#pragma once

#include <stdint.h>
#include <stddef.h>

#define WIFI_SERIAL_MODE {0, 9, 0, 0, 0, 0} // 115200 bps 8n1
#define WIFI_SERIAL_BUFFER_SIZE 256
#define STX 0x02
#define ETX 0x03
#define CHUNK_SIZE 64

enum {
    WIFI_STATUS_DISCONNECTED = 0x00,
    WIFI_STATUS_CONNECTING = 0x01,
    WIFI_STATUS_CONNECTED = 0x02,
    WIFI_STATUS_ERROR = 0x03
};

enum {
    CMD_WIFI_STATUS = 0x01,
    CMD_WIFI_SCAN = 0x02,
    CMD_WIFI_CONNECT = 0x03,
    CMD_WIFI_DISCONNECT = 0x04,
    CMD_WIFI_GET_IP = 0x05,
    CMD_TRANSCEIVE = 0x06,
    CMD_ACK = 0x07,
    CMD_RESPONSE_OK = 0xFE,
    CMD_RESPONSE_ERR = 0xFF
};

#pragma pack(push, 1)
typedef struct {
    uint8_t  start_byte; // Always 0x02 (STX - Start of Text)
    uint8_t  command_id;
    uint16_t payload_len;
} __attribute__((packed)) msg_header_t;

typedef struct {
    uint8_t  checksum;
    uint8_t  end_byte; // Always 0x03 (ETX - End of Text)
} __attribute__((packed)) msg_footer_t;

// Status Response
typedef struct {
    uint8_t status;
} __attribute__((packed)) wifi_status_response_t;

// Scan Response
typedef struct {
    char ssid[32];
    int8_t rssi;
} __attribute__((packed)) wifi_scan_item_t;

typedef struct {
    uint16_t count;
    wifi_scan_item_t items[]; // it has count items
} __attribute__((packed)) wifi_scan_list_t;

// Connect Payload
typedef struct {
    char ssid[32];
    char password[64];
} __attribute__((packed)) wifi_connect_payload_t;

// Get IP Response
typedef struct {
    char ip_address[16];
} __attribute__((packed)) wifi_get_ip_response_t;

// Transceive Payload
typedef struct {
    uint16_t data_len;
    uint8_t data[];
} __attribute__((packed)) wifi_transceive_payload_t;

// Transceive Response
typedef struct {
  uint16_t total_chunks;
  uint16_t current_chunk;
  uint16_t data_len;
  uint8_t data[];
} __attribute__((packed)) wifi_transceive_response_t;
#pragma pack(pop)

/**
 * Functions
 */
#ifdef __cplusplus
extern "C" {
#endif

int wifi_init();
int wifi_deinit();
int wifi_connect(const char * ssid, const char * password);
int wifi_disconnect();
uint8_t wifi_status();
int wifi_get_ip(char * buffer, uint32_t buffer_size);
wifi_scan_list_t * wifi_scan();
void wifi_free_scan_list();
uint8_t * wifi_transceive(const char * request, uint16_t * response_size);
void wifi_free_transceive_response(uint8_t * response);

#ifdef __cplusplus
}
#endif
