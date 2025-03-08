#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include "esp_ota_ops.h"
#include <HTTPClient.h>

const char* AP_SSID = "TANTAN-GENESIS";
const char* AP_PASS = "12345678";

Preferences preferences;
AsyncWebServer server(80);

const char* firmware_url = "https://raw.githubusercontent.com/hoanglinhtan/esp32_server/main/partition2.ino.bin"; // Thay bằng URL thực tế
//const char* firmware_url = "https://raw.githubusercontent.com/umeshwalkar/esp32rfid-bin/refs/heads/master/wifirfid_1.0_v2.bin";

void setup() {
    Serial.begin(115200);

    // Đọc thông tin WiFi từ Flash
    preferences.begin("wifi", false);
    String ssid = preferences.getString("ssid", "");
    String pass = preferences.getString("pass", "");
    preferences.end();

    if (ssid.length() > 0) {
        Serial.println("Trying to connect to saved WiFi...");
        WiFi.begin(ssid.c_str(), pass.c_str());
        if (WiFi.waitForConnectResult() == WL_CONNECTED) {
            Serial.println("Connected to WiFi!");
            downloadAndUpdateFirmware();
            return;
        }
    }

    // Nếu không kết nối được, bật WiFi AP
    WiFi.softAP(AP_SSID, AP_PASS);
    Serial.println("WiFi AP Started!");

    // Webserver xử lý nhập WiFi
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/html", R"rawliteral(
            <form action='/save' method='post'>
                WiFi Name: <input type='text' name='ssid'><br>
                Password: <input type='password' name='pass'><br>
                <input type='submit' value='Save'>
            </form>
        )rawliteral");
    });

    server.on("/save", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (request->hasParam("ssid", true) && request->hasParam("pass", true)) {
            String ssid = request->getParam("ssid", true)->value();
            String pass = request->getParam("pass", true)->value();

            preferences.begin("wifi", false);
            preferences.putString("ssid", ssid);
            preferences.putString("pass", pass);
            preferences.end();

            request->send(200, "text/plain", "WiFi saved. Restarting...");
            delay(1000);
            ESP.restart();
        } else {
            request->send(400, "text/plain", "Missing parameters!");
        }
    });

    server.begin();
}

void loop() {}

// ========== TẢI FILE .BIN VÀ UPDATE ==========
void downloadAndUpdateFirmware() {
  const esp_partition_t *running_partition = esp_ota_get_running_partition();
    if (running_partition != NULL) {
        Serial.print("Đang chạy trên phân vùng: %s");
        Serial.println(running_partition->label);
        Serial.print("Loại: %d, Dưới loại: %d");
        Serial.print(running_partition->type);
        Serial.println(running_partition->subtype);

        Serial.print("Địa chỉ bắt đầu: 0x%06x, Kích thước: 0x%06x");
        Serial.print(running_partition->address);
        Serial.println(running_partition->size);

    } else {
        Serial.println("Không thể lấy thông tin phân vùng đang chạy.");
    }

   // list all partitions
    Serial.println("Listing all partitions:");

    // Iterate over all partition types
    for (int type = ESP_PARTITION_TYPE_APP; type <= ESP_PARTITION_TYPE_DATA; type++) {
        // Cast 'type' to 'esp_partition_type_t'
        esp_partition_iterator_t it = esp_partition_find((esp_partition_type_t)type, ESP_PARTITION_SUBTYPE_ANY, NULL);
        while (it != NULL) {
            const esp_partition_t *part = esp_partition_get(it);
            if (part != NULL) {
              //  ESP_LOGI("Partition Info", "Name: %s, Type: %d, Subtype: %d, Address: 0x%06x, Size: 0x%06x",
                //         part->label, part->type, part->subtype, part->address, part->size);
                Serial.print(part->label);
                Serial.print(" ");
                Serial.print(part->type);
                Serial.print(" ");
                Serial.print(part->subtype);
                Serial.print(" ");
                Serial.print(part->address);
                Serial.print(" ");
                Serial.print(part->size);
                Serial.print(" ");
                Serial.println();
            }
            it = esp_partition_next(it);
        }
        esp_partition_iterator_release(it);
    }

    HTTPClient http;
    http.begin(firmware_url);
    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) {
        Serial.printf("HTTP request failed: %d\n", httpCode);
        return;
    }

    int contentLength = http.getSize();
    if (contentLength <= 0) {
        Serial.println("Invalid content length!");
        return;
    }
    Serial.println("ContentLength");
    Serial.println(contentLength);

    // list all partition

    const esp_partition_t* update_partition = esp_ota_get_next_update_partition(NULL);
    Serial.println(update_partition->label);
    if (update_partition == NULL) {
        Serial.println("No OTA partition found!");
        return;
    }

    esp_ota_handle_t ota_handle;
    if (esp_ota_begin(update_partition, contentLength, &ota_handle) != ESP_OK) {
        Serial.println("OTA Begin Failed!");
        return;
    }

    WiFiClient* stream = http.getStreamPtr();
    while (stream->available() == 0) {
        delay(100);
        Serial.print(".");
    }
    Serial.println("\n✅ Dữ liệu đã sẵn sàng để đọc!");

    uint8_t buffer[1024];

    Serial.println("\n✅ OTA handle");
    Serial.println(ota_handle);
    
    int totalBytesRead = 0;
    int time = 1;
    while (http.connected()) {
        Serial.println(sizeof(buffer));
        size_t bytesRead = stream->readBytes(buffer, sizeof(buffer));
        if (bytesRead > 0) {
          int a = esp_ota_write(ota_handle, buffer, bytesRead);
          if (a != ESP_OK) {
              Serial.println("OTA Write Failed!");
              return;
          }
          time++;
          totalBytesRead += bytesRead;
          Serial.println(totalBytesRead);
          if (totalBytesRead >= contentLength) {
            break;
          }
        }
        delay(10);
    }

    Serial.print("ota_handle: ");
    Serial.println(ota_handle);

    if (esp_ota_end(ota_handle) != ESP_OK) {
        Serial.println("OTA End Failed!");
        return;
    }

    Serial.println("Firmware update successful!");

    if (esp_ota_set_boot_partition(update_partition) == ESP_OK) {
        Serial.println("Rebooting to new firmware...");
        esp_restart();
    } else {
        Serial.println("Failed to set boot partition!");
    }
}