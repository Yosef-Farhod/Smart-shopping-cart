#include <WiFi.h>
#include <HTTPClient.h>
#include <FirebaseESP32.h>
#include <HX711.h>
#include <WebServer.h>

const char *ssid = "Yosef";
const char *password = "28072004";

// بيانات Firebase
#define API_KEY "AIzaSyCCi6Yvyfh7zPY_DqczkMcBUFdkmmI8xTA"
#define DATABASE_URL "https://smart-cart-f9c56-default-rtdb.firebaseio.com/"
#define USER_EMAIL "yoseffarhod@gmail.com"
#define USER_PASSWORD "y28072004"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

String serial = "";
float weight = 0;
float coins = 0.0;

#define BASE_PATH "/users/fj@fj,com/shelf_settings/"
#define PRODUCT_PATH "/products/CyhYDpfJgNTcpQpMcfWK/"
String shelf_esp32_ip = "";
float shelf_total_weight = 0;

#define DT1 18
#define SCK1 19
#define DT2 21
#define SCK2 22
#define BUZZER_PIN 23
#define BUZZER_THRESHOLD 4500

HX711 scale1;
HX711 scale2;

float previous_weight = 0.0;
bool waiting_for_scan_ok = false;
bool scan_ok_received = false;
unsigned long scan_request_time = 0;
const unsigned long SCAN_TIMEOUT = 20000;
const float MIN_WEIGHT_DIFF = 30.0;
float weight_sensor_setting = 350;

WebServer server(80);

void setup()
{
    Serial.begin(115200);

    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.print(".");
        delay(500);
    }
    Serial.println();
    Serial.print("Connected with IP: ");
    Serial.println(WiFi.localIP());

    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);

    scale1.begin(DT1, SCK1);
    scale2.begin(DT2, SCK2);

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

    if (Firebase.getString(&fbdo, BASE_PATH "esp32_ip"))
        shelf_esp32_ip = fbdo.stringData();

    if (Firebase.getFloat(&fbdo, BASE_PATH "total_weight"))
        shelf_total_weight = fbdo.floatData();

    if (Firebase.getString(&fbdo, PRODUCT_PATH "serial"))
        serial = fbdo.stringData();

    if (Firebase.getFloat(&fbdo, PRODUCT_PATH "weight"))
        weight = fbdo.floatData();

    if (Firebase.getFloat(&fbdo, BASE_PATH "min_weight_diff"))
        weight_sensor_setting = fbdo.floatData();

    // جلب رصيد العملات الرقمية من فير بيز
    if (Firebase.getFloat(&fbdo, "/users/fj@fj,com/coins"))
    {
        coins = fbdo.floatData();
        Serial.print("💰 رصيد العملات الرقمية: ");
        Serial.println(coins, 2);
    }
    else
    {
        Serial.print("❌ فشل في جلب رصيد العملات: ");
        Serial.println(fbdo.errorReason());
    }

    Serial.println("📦 إعدادات الرف:");
    Serial.println("ESP32 IP: " + shelf_esp32_ip);
    Serial.println("Serial: " + serial);
    Serial.println("Weight: " + String(weight));
    Serial.println("Min Weight Diff: " + String(weight_sensor_setting));

    scale1.set_scale(weight_sensor_setting);
    scale2.set_scale(weight_sensor_setting);

    if (scale1.is_ready())
        scale1.tare();
    if (scale2.is_ready())
        scale2.tare();

    if (scale1.is_ready() || scale2.is_ready())
    {
        float w1 = scale1.is_ready() ? scale1.get_units(5) : 0.0;
        float w2 = scale2.is_ready() ? scale2.get_units(5) : 0.0;
        previous_weight = w1 + w2;
    }

    server.on("/update", handleUpdate);
    server.begin();
    Serial.println("🚀 Web server for shelf started");
}

void handleUpdate()
{
    Serial.println("📥 Received request on /update (shelf)");

    if (server.hasArg("scan") && server.arg("scan") == "ok" && server.hasArg("serial"))
    {
        scan_ok_received = true;
        Serial.println("✅ تم استقبال scan ok من السلة.");
        server.send(200, "text/plain", "Scan OK received by shelf");
    }
    else
    {
        server.send(400, "text/plain", "Invalid request");
    }
}

void checkScanOk()
{
    if (Serial.available())
    {
        String input = Serial.readStringUntil('\n');
        input.trim();
        if (input.equalsIgnoreCase("scan ok"))
        {
            scan_ok_received = true;
            Serial.println("✅ تم استقبال scan ok من الشاشة.");
        }
    }
}

void loop()
{
    server.handleClient();
    checkScanOk();

    static bool first_run = true;
    static unsigned long first_run_start = 0;
    if (first_run)
    {
        if (first_run_start == 0)
            first_run_start = millis();
        if (millis() - first_run_start < 5000)
            return;
        else
            first_run = false;
    }

    float weight1 = scale1.is_ready() ? scale1.get_units(10) : 0.0;
    float weight2 = scale2.is_ready() ? scale2.get_units(10) : 0.0;
    float totalWeight = weight1 + weight2;

    Serial.printf("وزن 1: %.2f جم | وزن 2: %.2f جم | الإجمالي: %.2f جم\n", weight1, weight2, totalWeight);

    if (totalWeight > BUZZER_THRESHOLD)
        digitalWrite(BUZZER_PIN, HIGH);
    else
        digitalWrite(BUZZER_PIN, LOW);

    if (!isnan(weight1) && !isnan(weight2) && (weight1 != 0.0 || weight2 != 0.0))
    {
        float diff = previous_weight - totalWeight;

        if (abs(diff) >= MIN_WEIGHT_DIFF)
        {
            int product_count = round(diff / weight);
            if (product_count != 0)
            {
                waiting_for_scan_ok = true;
                scan_request_time = millis();

                if (product_count > 0)
                    Serial.printf("✅ %d تم أخذها\n", product_count);
                else
                    Serial.printf("🔄 %d تم إرجاعها\n", abs(product_count));

                Serial.printf("📦 Barcode: %s\n", serial.c_str());

                if (WiFi.status() == WL_CONNECTED && shelf_esp32_ip.length() >= 7)
                {
                    WiFiClient client;
                    HTTPClient http;
                    String url = "http://" + shelf_esp32_ip +
                                 "/update?serial=" + serial +
                                 "&count=" + String(product_count) +
                                 "&reading=" + String(totalWeight, 2) +
                                 "&weight=" + String(weight, 2);
                    Serial.print("🔗 إرسال طلب إلى السلة: ");
                    Serial.println(url);

                    http.begin(client, url);
                    int httpCode = http.GET();
                    if (httpCode > 0)
                    {
                        String response = http.getString();
                        Serial.println("📡 Server response: " + response);
                    }
                    else
                    {
                        Serial.println("❌ فشل في الاتصال بالسيرفر");
                    }
                    http.end();
                }
                else
                {
                    Serial.println("❌ IP غير صالح أو لا يوجد اتصال WiFi.");
                }

                previous_weight = totalWeight;
            }
        }
    }

    if (waiting_for_scan_ok)
    {
        if (scan_ok_received)
        {
            digitalWrite(BUZZER_PIN, LOW);
            waiting_for_scan_ok = false;
            scan_ok_received = false;
            Serial.println("✅ تم استلام Scan OK وإيقاف البازر");
        }
        else if (millis() - scan_request_time > SCAN_TIMEOUT)
        {
            digitalWrite(BUZZER_PIN, HIGH);
            Serial.println("⏰ انتهى وقت الانتظار ولم يتم استلام Scan OK");
        }
    }
    else
    {
        digitalWrite(BUZZER_PIN, LOW);
    }

    delay(1000);
}