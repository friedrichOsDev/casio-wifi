#include <HTTPClient.h>
#include <WiFi.h>

#define RXD2 16
#define TXD2 17
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
  uint8_t start_byte;  // Always 0x02 (STX - Start of Text)
  uint8_t command_id;
  uint16_t payload_len;
} __attribute__((packed)) msg_header_t;

typedef struct {
  uint8_t checksum;
  uint8_t end_byte;  // Always 0x03 (ETX - End of Text)
} __attribute__((packed)) msg_footer_t;

// Status Response
typedef struct {
  uint8_t status;
} __attribute__((packed)) wifi_status_response_t;

// Connect Payload
typedef struct {
  char ssid[32];
  char password[64];
} __attribute__((packed)) wifi_connect_payload_t;

// Get IP Response
typedef struct {
  char ip_address[16];
} __attribute__((packed)) wifi_get_ip_response_t;

// Scan Response
typedef struct {
  char ssid[32];
  int8_t rssi;
} __attribute__((packed)) wifi_scan_item_t;

typedef struct {
  uint16_t count;
  wifi_scan_item_t items[];  // it has count items
} __attribute__((packed)) wifi_scan_list_t;

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

uint16_t swap_uint16(uint16_t val) {
  return (val << 8) | (val >> 8);
}

uint8_t calculate_checksum(msg_header_t* header, uint8_t* payload) {
  uint8_t checksum = 0;
  checksum ^= header->command_id;                     // Command Id
  uint8_t* len_ptr = (uint8_t*)&header->payload_len;  // Payload Len
  checksum ^= len_ptr[0];
  checksum ^= len_ptr[1];

  if (payload != nullptr && header->payload_len > 0) {
    for (uint16_t i = 0; i < header->payload_len; i++) {
      checksum ^= payload[i];  // Payload Bytes
    }
  }

  return checksum;
}

bool wait_for_casio_ack(uint32_t timeout_ms) {
  uint32_t start_ms = millis();

  while (millis() - start_ms < timeout_ms) {
    if (Serial2.available() > 0) {
      uint8_t c = Serial2.read();
      Serial.printf("Byte: %02X\n", c);

      if (c == STX) {
        msg_header_t ack_header;
        ack_header.start_byte = c;

        uint32_t header_timeout = millis();
        size_t read_bytes = 1;
        uint8_t* header_ptr = (uint8_t*)&ack_header;

        while (read_bytes < sizeof(msg_header_t) && (millis() - header_timeout < 50)) {
          if (Serial2.available()) {
            header_ptr[read_bytes++] = Serial2.read();
          }
        }

        if (read_bytes == sizeof(msg_header_t) && ack_header.command_id == 0x07 && ack_header.payload_len == 0) {
          msg_footer_t ack_footer;
          uint8_t* footer_ptr = (uint8_t*)&ack_footer;
          size_t footer_bytes = 0;
          header_timeout = millis();

          while (footer_bytes < sizeof(msg_footer_t) && (millis() - header_timeout < 50)) {
            if (Serial2.available()) {
              footer_ptr[footer_bytes++] = Serial2.read();
            }
          }
          
          if (ack_footer.checksum == calculate_checksum(&ack_header, nullptr) && ack_footer.end_byte == ETX) {
            return true;
          }
        }
      }
    }
    yield();
  }
  return false;
}

void send_packet(uint8_t cmd_id, uint8_t* payload, uint16_t len) {
  msg_header_t header = { STX, cmd_id, len };
  uint8_t cs = calculate_checksum(&header, payload);
  msg_footer_t footer = { cs, ETX };
  header.payload_len = swap_uint16(len);  // Convert to BIG-ENDIAN for CASIO

  Serial2.write((uint8_t*)&header, sizeof(msg_header_t));
  if (len > 0 && payload != nullptr) {
    Serial2.write(payload, len);
  }
  Serial2.write((uint8_t*)&footer, sizeof(msg_footer_t));
  Serial2.flush();
}

void send_transceive_packet(uint8_t cmd_id, uint8_t* payload, uint16_t len) {
  uint16_t total_chunks = (len + CHUNK_SIZE - 1) / CHUNK_SIZE;

  for (uint16_t i = 0; i < total_chunks; i++) {
    uint16_t current_offset = i * CHUNK_SIZE;
    uint16_t current_data_len = (len - current_offset > CHUNK_SIZE) ? CHUNK_SIZE : (len - current_offset);

    uint16_t payload_struct_size = sizeof(uint16_t) * 3 + current_data_len;
    wifi_transceive_response_t* chunk_payload = (wifi_transceive_response_t*)malloc(payload_struct_size);

    if (!chunk_payload) return;

    chunk_payload->total_chunks = total_chunks;
    chunk_payload->current_chunk = i;
    chunk_payload->data_len = current_data_len;
    memcpy(chunk_payload->data, payload + current_offset, current_data_len);

    chunk_payload->total_chunks = swap_uint16(total_chunks);
    chunk_payload->current_chunk = swap_uint16(i);
    chunk_payload->data_len = swap_uint16(current_data_len);

    send_packet(CMD_RESPONSE_OK, (uint8_t*)chunk_payload, payload_struct_size);
    free(chunk_payload);

    if (!wait_for_casio_ack(10000)) {
      Serial.println("Error: Casio ACK timeout on chunk");
      return;
    }
    delay(10);
  }
}

void send_ack(uint8_t cmd_id) {
  delay(50);
  send_packet(cmd_id, nullptr, 0);
}

void send_status() {
  wifi_status_response_t payload;
  memset(&payload, 0, sizeof(wifi_status_response_t));

  wl_status_t current = WiFi.status();
  if (current == WL_CONNECTED) {
    payload.status = WIFI_STATUS_CONNECTED;
  } else if (current == WL_IDLE_STATUS || current == WL_SCAN_COMPLETED) {
    payload.status = WIFI_STATUS_CONNECTING;
  } else if (current == WL_DISCONNECTED || current == WL_NO_SHIELD) {
    payload.status = WIFI_STATUS_DISCONNECTED;
  } else {
    payload.status = WIFI_STATUS_ERROR;
  }

  send_packet(CMD_RESPONSE_OK, (uint8_t*)&payload, sizeof(wifi_status_response_t));
}

void send_ip() {
  wifi_get_ip_response_t payload;
  memset(&payload, 0, sizeof(wifi_get_ip_response_t));

  if (WiFi.status() == WL_CONNECTED) {
    strncpy(payload.ip_address, WiFi.localIP().toString().c_str(), sizeof(payload.ip_address) - 1);
  } else {
    strncpy(payload.ip_address, "0.0.0.0", sizeof(payload.ip_address) - 1);
  }

  send_packet(CMD_RESPONSE_OK, (uint8_t*)&payload, sizeof(wifi_get_ip_response_t));
}

void send_scan() {
  int16_t n = WiFi.scanNetworks();

  if (n < 0) {
    send_ack(CMD_RESPONSE_ERR);
    return;
  }

  uint16_t list_count = (uint16_t)n;
  uint32_t payload_size = sizeof(uint16_t) + (list_count * sizeof(wifi_scan_item_t));

  uint8_t* payload = (uint8_t*)malloc(payload_size);
  if (!payload) {
    send_ack(CMD_RESPONSE_ERR);
    return;
  }

  *((uint16_t*)payload) = swap_uint16(list_count);  // Convert to BIG-ENDIAN for CASIO

  wifi_scan_item_t* items = (wifi_scan_item_t*)(payload + sizeof(uint16_t));
  for (int i = 0; i < n; ++i) {
    memset(items[i].ssid, 0, 32);
    strncpy(items[i].ssid, WiFi.SSID(i).c_str(), 32);
    items[i].rssi = (int8_t)WiFi.RSSI(i);
  }

  send_packet(CMD_RESPONSE_OK, payload, payload_size);

  free(payload);
  WiFi.scanDelete();
}

void send_transceive_response(const char* message) {
  uint16_t msg_len = strlen(message);
  uint16_t total_payload_len = sizeof(uint16_t) + msg_len;

  uint8_t* buffer = (uint8_t*)malloc(total_payload_len);
  if (!buffer) return;

  *((uint16_t*)buffer) = swap_uint16(msg_len);
  memcpy(buffer + sizeof(uint16_t), message, msg_len);

  Serial.printf("Info: Sending ACK and API Data (%d bytes)\n", total_payload_len);
  send_transceive_packet(CMD_RESPONSE_OK, buffer, total_payload_len);
  free(buffer);
}

void handle_transceive(uint8_t* incoming_payload, uint16_t incoming_len) {
  wifi_transceive_payload_t* req = (wifi_transceive_payload_t*)incoming_payload;
  uint16_t data_len = req->data_len;

  // this isn't the final version of the transceive handler
  // the HTTP request is a test to verify the connection (CASIO -> ESP32 -> INTERNET)

  String requestString = String((char*)req->data).substring(0, data_len);

  if (requestString.startsWith("GET ")) {
    String url = requestString.substring(4);
    Serial.printf("API URL: %s\n", url.c_str());

    HTTPClient http;
    http.begin(url);
    int httpCode = http.GET();

    if (httpCode > 0) {
      String result = http.getString();
      Serial.printf("Result: %s\n", result.c_str());
      send_transceive_response(result.c_str());
    } else {
      send_transceive_response("Error: HTTP Failed");
    }
    http.end();
  }
}

void setup() {
  pinMode(2, OUTPUT);                             // LED
  Serial.begin(115200);                           // Debug
  Serial2.begin(115200, SERIAL_8N1, RXD2, TXD2);  // Casio
}

void loop() {
  digitalWrite(2, (WiFi.status() == WL_CONNECTED) ? HIGH : LOW);

  if (Serial2.available() > 0) {
    if (Serial2.peek() != STX) {
      uint8_t junk = Serial2.read();
      Serial.printf("Info: Skipped byte 0x%02X\n", junk);
      return;
    }

    if (Serial2.available() >= sizeof(msg_header_t)) {
      msg_header_t header;
      Serial2.readBytes((uint8_t*)&header, sizeof(msg_header_t));
      if (header.start_byte != STX) {
        Serial.println("Error: No STX in Header");
        return;
      }

      uint8_t* payload_buffer = nullptr;
      if (header.payload_len > 0) {
        payload_buffer = (uint8_t*)malloc(header.payload_len);
        uint32_t start_ms = millis();
        size_t bytes_read = 0;
        while (bytes_read < header.payload_len && (millis() - start_ms < 500)) {
          if (Serial2.available() > 0) {
            payload_buffer[bytes_read++] = Serial2.read();
          }
        }
        if (bytes_read < header.payload_len) {
          Serial.println("Error: Payload timeout");
          free(payload_buffer);
          return;
        }
      }

      msg_footer_t footer;
      uint32_t footer_start = millis();
      while (Serial2.available() < sizeof(msg_footer_t) && (millis() - footer_start < 200))
        ;
      if (Serial2.available() < sizeof(msg_footer_t)) {
        Serial.println("Error: Footer timeout");
        if (payload_buffer) free(payload_buffer);
        return;
      }
      Serial2.readBytes((uint8_t*)&footer, sizeof(msg_footer_t));

      uint8_t calc_cs = calculate_checksum(&header, payload_buffer);

      if (footer.checksum == calc_cs && footer.end_byte == ETX) {
        delay(10);

        switch (header.command_id) {
          case CMD_WIFI_STATUS:  // Done
            Serial.printf("Info: Send Status\n");
            send_status();
            break;

          case CMD_WIFI_CONNECT:  // Done
            if (payload_buffer) {
              wifi_connect_payload_t* conn = (wifi_connect_payload_t*)payload_buffer;
              Serial.printf("Info: Connect to SSID: %s\n", conn->ssid);
              WiFi.begin(conn->ssid, conn->password);
              send_ack(CMD_RESPONSE_OK);
            }
            break;

          case CMD_WIFI_DISCONNECT:  // Done
            Serial.printf("Info: Disconnect from WiFi\n");
            WiFi.disconnect();
            send_ack(CMD_RESPONSE_OK);
            break;

          case CMD_WIFI_GET_IP:  // Done
            Serial.printf("Info: Send IP\n");
            send_ip();
            break;

          case CMD_WIFI_SCAN:  // Done
            Serial.printf("Info: Send WiFi Scan\n");
            send_scan();
            break;

          case CMD_TRANSCEIVE:
            Serial.printf("Info: Received %d Bytes Data\n", header.payload_len);
            handle_transceive(payload_buffer, header.payload_len);
            break;

          default:
            Serial.printf("Error: Unknown Command: 0x%02X\n", header.command_id);
            send_ack(CMD_RESPONSE_ERR);
            break;
        }
      } else {
        Serial.println("Error: Checksum or ETX mismatch");
        send_ack(CMD_RESPONSE_ERR);
      }

      if (payload_buffer) free(payload_buffer);
    }
  }
}