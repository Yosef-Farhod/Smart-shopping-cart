//استقيال الكود الخاص بحساس الوزن HX711 مع ESP3
// استقيال قراءه حساس الوزن من esp8266 و ارسالها عن طريق الشبكه المحلي

#include <WiFi.h>
#include <WebServer.h>

const char* ssid = "ESP32_Test";
const char* password = "12345678";

WebServer server(80);

float latestWeight = 0.0;

void handleUpdate() {
  if (server.hasArg("weight")) {
    latestWeight = server.arg("weight").toFloat();
    Serial.print("Received weight: ");
    Serial.println(latestWeight);
    server.send(200, "text/plain", "Weight received");
  } else {
    server.send(400, "text/plain", "Missing weight");
  }
}

void setup() {
  Serial.begin(115200);
  WiFi.softAP(ssid, password);
  Serial.println("ESP32 Access Point Started");
  Serial.print("IP Address: ");
  Serial.println(WiFi.softAPIP());

  server.on("/update", handleUpdate);
  server.begin();
}

void loop() {
  server.handleClient();
}