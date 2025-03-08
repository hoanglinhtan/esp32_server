#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include "esp_ota_ops.h"
#include "esp_partition.h"

// Cấu hình Access Point (WiFi phát ra)
const char* AP_SSID = "TANTAN_OTA";
const char* AP_PASS = "12345678"; // Để trống "" nếu không cần mật khẩu

// Tạo WebServer chạy trên cổng 80
AsyncWebServer server(80);

// HTML trang web nhập mật khẩu
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>ESP32 OTA Update</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial, sans-serif; text-align: center; margin-top: 50px; }
        input { padding: 10px; font-size: 16px; margin: 10px; }
        button { padding: 10px; font-size: 18px; cursor: pointer; }
    </style>
</head>
<body>
    <h2>ESP32 OTA Update</h2>
    <p>Enter password to switch partition:</p>
    <input type="password" id="password" placeholder="Enter password (1-9)">
    <button onclick="submitPassword()">Submit</button>
    <p id="status"></p>
    <br>
    <script>
        function submitPassword() {
            var pw = document.getElementById("password").value;
            fetch("/validate?password=" + pw)
                .then(response => response.text())
                .then(data => document.getElementById("status").innerText = data);
        }
    </script>
</body>
</html>
)rawliteral";

// Xử lý nhập mật khẩu
void handlePassword(AsyncWebServerRequest *request) {
    if (request->hasParam("password")) {
        String inputPassword = request->getParam("password")->value();
        if (inputPassword == "1234") { // Đúng mật khẩu
            request->send(200, "text/plain", "Password correct. Switching partition...");
            delay(2000);
            switchToPartition1();
        } else {
            request->send(200, "text/plain", "Wrong password!");
        }
    } else {
        request->send(400, "text/plain", "Missing password parameter");
    }
}

// Chuyển ESP32 về phân vùng 1 (`app0`)
void switchToPartition1() {
    const esp_partition_t *partition = esp_ota_get_next_update_partition(NULL);
    Serial.write(partition->label);
    Serial.write("Switching....");
    if (partition) {
        delay(2000);
        esp_ota_set_boot_partition(partition);
        Serial.write("Restart....");
        ESP.restart();
    }
}

void setup() {
    Serial.begin(115200);

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

    // Khởi động chế độ Access Point
    WiFi.softAP(AP_SSID, AP_PASS);
    Serial.println("WiFi AP Started!");
    Serial.print("ESP32 IP Address: ");
    Serial.println(WiFi.softAPIP());

    // Cấu hình WebServer
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/html", index_html);
    });

    server.on("/validate", HTTP_GET, handlePassword);

    server.begin();
}

void loop() {
    // Không làm gì trong loop vì webserver chạy nền
}
