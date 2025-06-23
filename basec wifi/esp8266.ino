//قراءه // حساس الوزن HX711 مع ESP8266 وإرسال البيانات إلى خادم HTTP
// هذا الكود يستخدم مكتبة HX711 لقراءة الوزن من حساس HX711 ويرسل البيانات إلى خادم HTTP عبر WiFi.



#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

const char* ssid = "Yosef";
const char* password = "28072004";
const char* host = "192.168.4.1";

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi");
  Serial.print("IP Address ESP8266: ");
  Serial.println(WiFi.localIP());


}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    float weight = 5.5; // يمكنك تغيير القيمة هنا لأي قراءة تريد إرسالها

    WiFiClient client;
    HTTPClient http;
    String url = "http://" + String(host) + "/update?weight=" + String(weight, 2);
    http.begin(client, url);
    int httpCode = http.GET();

    if (httpCode > 0) {
      String response = http.getString();
      Serial.println("Server response: " + response);
    } else {
      Serial.println("Connection failed");
    }

    http.end();
  }

  delay(2000);
}