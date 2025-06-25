#include <WiFi.h>
#include <WebServer.h>

const char* ssid = "Yosef";
const char* password = "28072004";

WebServer server(80);

// عند استقبال تحديث من الرف
void handleUpdate() {
  if (server.hasArg("serial")) {
    String serial = server.arg("serial");
    String name = server.hasArg("name") ? server.arg("name") : "";
    String price = server.hasArg("price") ? server.arg("price") : "";

    Serial.println("📥 استقبلت من الرف:");
    Serial.println("🔸 Serial: " + serial);
    Serial.println("🔸 Name: " + name);
    Serial.println("🔸 Price: " + price);

    // الرد للتأكيد
    server.send(200, "text/plain", "status=ok");
  } else {
    server.send(400, "text/plain", "Missing serial");
  }
}

void setup() {
  Serial.begin(115200);

  WiFi.begin(ssid, password);
  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ Connected to WiFi");
  Serial.print("ESP32 IP: ");
  Serial.println(WiFi.localIP());  // 👈 استخدم هذا الـ IP في ESP8266

  server.on("/update", handleUpdate);
  server.begin();
  Serial.println("🌐 HTTP Server بدأ");
}

void loop() {
  server.handleClient();
}
