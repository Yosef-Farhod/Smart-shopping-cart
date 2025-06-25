#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

const char* ssid = "Yosef";
const char* password = "28072004";

String shelf_product_serial = "XYZ123";
String shelf_product_name = "تفاح";
float shelf_product_price = 12.5;

String smart_cart_ip = "192.168.43.21";  // عدّله حسب IP الـ ESP32 بعد الاتصال

unsigned long lastSendTime = 0;
const unsigned long timeout = 10000;  // مهلة 10 ثوانٍ للرد
bool waitingForResponse = false;

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ Connected to WiFi");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  delay(2000);
  sendProductToCart();
}

void sendProductToCart() {
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClient client;
    HTTPClient http;

    String url = "http://" + smart_cart_ip + "/update?serial=" + shelf_product_serial +
                 "&name=" + shelf_product_name +
                 "&price=" + String(shelf_product_price, 2);

    Serial.println("📤 إرسال المنتج إلى السلة: " + url);

    http.begin(client, url);

    int httpCode = http.GET();
    if (httpCode > 0) {
      String response = http.getString();
      Serial.println("✅ رد السلة: " + response);
      waitingForResponse = false;
    } else {
      Serial.println("⏳ لم يتم الرد من السلة الآن... سأنتظر");
      lastSendTime = millis();
      waitingForResponse = true;
    }

    http.end();
  }
}

void loop() {
  if (waitingForResponse && millis() - lastSendTime > timeout) {
    Serial.println("🔔 البازر شغال (محاكاة): لم يتم الرد خلال المهلة");
    waitingForResponse = false;
  }
}
