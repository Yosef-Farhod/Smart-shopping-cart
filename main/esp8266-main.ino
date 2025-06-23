#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <FirebaseESP8266.h>

const char* ssid = "Yosef";
const char* password = "28072004";
const char* host = "192.168.43.21"; // IP ESP32

#define API_KEY "AIzaSyCCi6Yvyfh7zPY_DqczkMcBUFdkmmI8xTA"
#define DATABASE_URL "https://smart-cart-f9c56-default-rtdb.firebaseio.com/"
#define USER_EMAIL "yoseffarhod@gmail.com"
#define USER_PASSWORD "y28072004"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

String name = "";
float price = 0;
// float weight = 0; // حذف الوزن
String serial = "";
String shelf = "";

void setup() {
  Serial.begin(115200);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());

  config.api_key = API_KEY;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  config.database_url = DATABASE_URL;
  Firebase.reconnectNetwork(true);
  fbdo.setBSSLBufferSize(1024, 1024);
  fbdo.setResponseSize(1024);
  Firebase.begin(&config, &auth);
  Firebase.setDoubleDigits(5);
  config.timeout.serverResponse = 10 * 1000;

  // جلب بيانات المنتج من فايربيز
  if (Firebase.getString(fbdo, "/products/CyhYDpfJgNTcpQpMcfWK/name"))
    name = fbdo.stringData();
  if (Firebase.getFloat(fbdo, "/products/CyhYDpfJgNTcpQpMcfWK/price"))
    price = fbdo.floatData();
  if (Firebase.getString(fbdo, "/products/CyhYDpfJgNTcpQpMcfWK/serial"))
    serial = fbdo.stringData();
  if (Firebase.getString(fbdo, "/products/CyhYDpfJgNTcpQpMcfWK/shelf"))
    shelf = fbdo.stringData();

  // عرض بيانات المنتج
  Serial.println("بيانات المنتج:");
  Serial.print("الاسم: ");
  Serial.println(name);
  Serial.print("السعر: ");
  Serial.println(price);
  Serial.print("الرقم التسلسلي: ");
  Serial.println(serial);
  Serial.print("الرف: ");
  Serial.println(shelf);
  Serial.println("----------------------");
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    // إرسال الرقم التسلسلي فقط
    WiFiClient client;
    HTTPClient http;
    String url = "http://" + String(host) + "/update?serial=" + serial;
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