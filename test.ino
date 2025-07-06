#include <WiFi.h>
#include <HTTPClient.h>
#include <FirebaseESP32.h>
#include <HX711.h>

const char *ssid = "Yosef";
const char *password = "28072004";

#define API_KEY "AIzaSyCCi6Yvyfh7zPY_DqczkMcBUFdkmmI8xTA"
#define DATABASE_URL "https://smart-cart-f9c56-default-rtdb.firebaseio.com/"
#define USER_EMAIL "yoseffarhod@gmail.com"
#define USER_PASSWORD "y28072004"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

String serial = "";
float weight = 0;

#define BASE_PATH "/users/fj@fj,com/shelf_settings/"
#define PRODUCT_PATH "/products/CyhYDpfJgNTcpQpMcfWK/"
String shelf_esp32_ip = "";
float shelf_total_weight = 0;

// عدل الدبابيس حسب ESP32 إذا لزم الأمر
#define DT1 18
#define SCK1 19
#define DT2 21
#define SCK2 22
#define BUZZER_PIN 23
#define BUZZER_THRESHOLD 4900

HX711 scale1;
HX711 scale2;

float previous_weight = 0.0;
bool waiting_for_scan_ok = false;
unsigned long scan_request_time = 0;
const unsigned long SCAN_TIMEOUT = 20000;

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

    Serial.println("تم تهيئة الحساسين وتصفير الوزن.");

    config.api_key = API_KEY;
    config.database_url = DATABASE_URL;
    auth.user.email = USER_EMAIL;
    auth.user.password = USER_PASSWORD;
    Firebase.reconnectNetwork(true);
    fbdo.setBSSLBufferSize(1024, 1024);
    fbdo.setResponseSize(512);
    Firebase.begin(&config, &auth);
    Firebase.setDoubleDigits(5);
    config.timeout.serverResponse = 10 * 1000;

    if (Firebase.getString(&fbdo, BASE_PATH "esp32_ip"))
        shelf_esp32_ip = fbdo.stringData();
    else
        Serial.println("❌ فشل في جلب IP: " + fbdo.errorReason());
    shelf_esp32_ip = "192.168.43.19";

    if (Firebase.getFloat(&fbdo, BASE_PATH "total_weight"))
        shelf_total_weight = fbdo.floatData();

    if (Firebase.getString(&fbdo, PRODUCT_PATH "serial"))
        serial = fbdo.stringData();

    if (Firebase.getFloat(&fbdo, PRODUCT_PATH "weight"))
        weight = fbdo.floatData();

    Serial.println("📦 إعدادات الرف:");
    Serial.println("ESP32 IP: " + shelf_esp32_ip);
    Serial.print("الوزن الموجود في تعريف الوزن");
    Serial.println(shelf_total_weight);

    Serial.println("📄 بيانات المنتج:");
    Serial.print("الرقم التسلسلي: ");
    Serial.println(serial);
    Serial.print("الوزن: ");
    Serial.println(weight);
    Serial.println("----------------------");

    scale1.set_scale(shelf_total_weight);
    scale2.set_scale(shelf_total_weight);
    scale1.tare();
    scale2.tare();

    if (scale1.is_ready() || scale2.is_ready())
    {
        float weight1 = scale1.is_ready() ? scale1.get_units(5) : 0.0;
        float weight2 = scale2.is_ready() ? scale2.get_units(5) : 0.0;
        previous_weight = weight1 + weight2;
    }
    else
    {
        previous_weight = 0.0;
    }
}

void process_weight_change(float diff)
{
    int product_count = 0;
    if (weight > 0)
    {
        product_count = round(diff / weight);
    }
    else
    {
        Serial.println("❌ تحذير: الوزن غير صالح أو غير مُحمّل من Firebase!");
        return;
    }

    if (product_count == 0)
        return;

    if (product_count > 0)
        Serial.printf("✅ %d تم أخذها\n", product_count);
    else
        Serial.printf("🔄 %d تم إرجاعها\n", abs(product_count));

    Serial.printf("📦 Barcode: %s\n\n", serial.c_str());

    if (WiFi.status() == WL_CONNECTED)
    {
        WiFiClient client;
        HTTPClient http;

        String url = "http://" + shelf_esp32_ip +
                     "/update?serial=" + serial +
                     "&count=" + String(product_count) +
                     "&reading=" + String(previous_weight, 2);

        http.begin(client, url);
        int httpCode = http.GET();

        if (httpCode > 0)
        {
            String response = http.getString();
            Serial.println("📡 Server response: " + response);
            if (product_count > 0)
            {
                waiting_for_scan_ok = true;
                scan_request_time = millis();
            }
        }
        else
        {
            Serial.println("❌ فشل في الاتصال بالسيرفر");
        }

        http.end();
    }
}

void loop()
{
    float totalWeight = 0;
    float weight1 = 0, weight2 = 0;

    if (scale1.is_ready())
        weight1 = scale1.get_units(10);
    if (scale2.is_ready())
        weight2 = scale2.get_units(10);

    if (scale1.is_ready() || scale2.is_ready())
    {
        totalWeight = weight1 + weight2;

        Serial.print("وزن 1: ");
        Serial.print(weight1);
        Serial.print(" جم | وزن 2: ");
        Serial.print(weight2);
        Serial.print(" جم | الإجمالي: ");
        Serial.print(totalWeight);
        Serial.println(" جم");

        if (totalWeight > BUZZER_THRESHOLD)
        {
            digitalWrite(BUZZER_PIN, HIGH);
        }
        else
        {
            digitalWrite(BUZZER_PIN, LOW);
        }

        if (
            !isnan(weight1) && !isnan(weight2) &&
            (weight1 != 0.0 || weight2 != 0.0))
        {
            static float last_sent_weight = 0;
            float diff = totalWeight - last_sent_weight;
            if (abs(diff) >= 30)
            {
                int product_count = round(diff / weight);
                if (product_count != 0)
                {
                    if (product_count > 0)
                        Serial.printf("✅ %d تم أخذها\n", product_count);
                    else
                        Serial.printf("🔄 %d تم إرجاعها\n", abs(product_count));
                    Serial.printf("📦 Barcode: %s\n\n", serial.c_str());

                    if (WiFi.status() == WL_CONNECTED)
                    {
                        WiFiClient client;
                        HTTPClient http;
                        String url = "http://" + shelf_esp32_ip +
                                     "/update?serial=" + serial +
                                     "&count=" + String(product_count) +
                                     "&reading=" + String(totalWeight, 2) +
                                     "&weight=" + String(weight, 2);
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
                    last_sent_weight = totalWeight;
                }
            }
        }
    }
    else
    {
        Serial.println("خطأ: أحد الحساسات غير جاهز.");
    }

    if (waiting_for_scan_ok)
    {
        if (millis() - scan_request_time > SCAN_TIMEOUT)
        {
            digitalWrite(BUZZER_PIN, HIGH);
        }
        else
        {
            digitalWrite(BUZZER_PIN, LOW);
        }
    }
    else
    {
        digitalWrite(BUZZER_PIN, LOW);
    }

    delay(3000);
}