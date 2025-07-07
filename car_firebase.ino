#include <WiFi.h>
#include <WebServer.h>
#include <HX711.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <FirebaseESP32.h>

#define RXD2 21
#define TXD2 22

#define SCALE_DT 18
#define SCALE_SCK 19
#define BUZZER_PIN 23

const char *ssid = "Yosef";
const char *password = "28072004";

#define API_KEY "AIzaSyCCi6Yvyfh7zPY_DqczkMcBUFdkmmI8xTA"
#define DATABASE_URL "https://smart-cart-f9c56-default-rtdb.firebaseio.com/"
#define USER_EMAIL "yoseffarhod@gmail.com"
#define USER_PASSWORD "y28072004"

String latestSerial = "";
String latestCount = "";
String latestReading = "";
float latestWeight = 0.0;
int products_to_scan = 0;
int scanned_count = 0;
unsigned long scan_start_time = 0;
const unsigned long SCAN_TIMEOUT = 20000;
bool waiting_for_scan = false;

WebServer server(80);
HX711 scale;
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
float coins = 0.0;

const String scriptURL = "https://script.google.com/macros/s/AKfycbzFeE5NMAsaHIhrLmi4GCtSKLB6ZM2YkDLDd6pJmkFiZWBa-mrpB-QjeGcy848FQwV_/exec";
const String totalURL = "https://script.google.com/macros/s/AKfycbyV2zZqaaD0Te1n2GCYE82Gv-eycTawzO0-w7hSJNfCAq4WRBS64nTyPef_lS16v8FNCg/exec";

void handleUpdate()
{
    if (server.hasArg("serial"))
    {
        latestSerial = server.arg("serial");
        latestCount = server.hasArg("count") ? server.arg("count") : "";
        latestReading = server.hasArg("reading") ? server.arg("reading") : "";
        latestWeight = server.hasArg("weight") ? server.arg("weight").toFloat() : 0.0;
        products_to_scan = latestCount.toInt();
        scanned_count = 0;
        if (products_to_scan > 0)
        {
            waiting_for_scan = true;
            scan_start_time = millis();
        }
        server.send(200, "text/plain", "Data received");
    }
    else if (server.hasArg("scan") && server.arg("scan") == "ok" && server.hasArg("serial"))
    {
        if (waiting_for_scan && scanned_count < products_to_scan)
        {
            scanned_count++;
        }
        server.send(200, "text/plain", "Scan OK received");
    }
    else
    {
        server.send(400, "text/plain", "Missing serial");
    }
}

void handleRoot()
{
    server.send(200, "text/plain", "ESP32 Receiver is running");
}

void getProductInfo(String code)
{
    if (WiFi.status() != WL_CONNECTED)
    {
        return;
    }
    HTTPClient http;
    String url = scriptURL + "?code=" + code;
    http.begin(url);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    int httpCode = http.GET();
    if (httpCode > 0)
    {
        String response = http.getString();
        StaticJsonDocument<512> doc;
        DeserializationError error = deserializeJson(doc, response);
        if (!error)
        {
            String name = doc["name"];
            float price = doc["price"];
            if (coins >= price)
            {
                float new_coins = coins - price;
                if (Firebase.setFloat(fbdo, "/users/fj@fj,com/coins", new_coins))
                {
                    coins = new_coins;
                    sendToTotal(name, price);
                }
            }
            else
            {
                for (int i = 0; i < 3; i++)
                {
                    digitalWrite(BUZZER_PIN, HIGH);
                    delay(300);
                    digitalWrite(BUZZER_PIN, LOW);
                    delay(300);
                }
            }
        }
    }
    http.end();
}

void sendToTotal(String name, float price)
{
    HTTPClient http;
    name.replace(" ", "%20");
    String url = totalURL + "?action=add&name=" + name + "&price=" + String(price, 2);
    http.begin(url);
    int res = http.GET();
    if (res > 0)
    {
        String resText = http.getString();
    }
    http.end();
}

void setup()
{
    Serial.begin(115200);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
    config.api_key = API_KEY;
    config.database_url = DATABASE_URL;
    auth.user.email = USER_EMAIL;
    auth.user.password = USER_PASSWORD;
    Firebase.reconnectNetwork(true);
    fbdo.setBSSLBufferSize(1024, 1024);
    fbdo.setResponseSize(512);
    Firebase.begin(&config, &auth);
    Firebase.setDoubleDigits(5);
    config.timeout.serverResponse = 10000;
    if (Firebase.getFloat(&fbdo, "/users/fj@fj,com/coins"))
    {
        coins = fbdo.floatData();
    }
    server.on("/", handleRoot);
    server.on("/update", handleUpdate);
    server.begin();
    Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);
    scale.begin(SCALE_DT, SCALE_SCK);
    scale.set_scale(350);
    scale.tare();
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);
}

void loop()
{
    server.handleClient();
    if (Serial2.available())
    {
        String barcode = Serial2.readStringUntil('\n');
        barcode.trim();
        if (barcode.length() > 0)
        {
            getProductInfo(barcode);
            String shelf_ip = "192.168.43.19";
            String url = "http://" + shelf_ip + "/update?scan=ok&serial=" + barcode;
            WiFiClient client;
            HTTPClient http;
            http.begin(client, url);
            int httpCode = http.GET();
            if (httpCode > 0)
            {
                String response = http.getString();
            }
            http.end();
        }
    }
    static float last_weight = 0.0;
    float current_weight = scale.get_units(10);
    static bool buzzer_error_active = false;
    static unsigned long buzzer_error_start = 0;
    if (!isnan(current_weight))
    {
        if (waiting_for_scan && products_to_scan > 0)
        {
            if ((current_weight - last_weight) > 30 && scanned_count == 0)
            {
                if (!buzzer_error_active)
                {
                    buzzer_error_active = true;
                    buzzer_error_start = millis();
                    digitalWrite(BUZZER_PIN, HIGH);
                }
            }
            if (scanned_count >= products_to_scan)
            {
                buzzer_error_active = false;
                digitalWrite(BUZZER_PIN, LOW);
                waiting_for_scan = false;
            }
        }
    }
    last_weight = current_weight;
    delay(1000);
}