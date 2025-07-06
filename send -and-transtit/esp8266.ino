#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <HX711.h>

const char *ssid = "Yosef";
const char *password = "28072004";

// بيانات المنتج (ضع قيم ثابتة أو متغيرة حسب الحاجة)
String serial = "ABC123";
float weight = 100.0;

// إعدادات الرف
String shelf_esp32_ip = "192.168.43.19";
float shelf_total_weight = 2280.0;

#define DT1 D5
#define SCK1 D6
#define DT2 D7
#define SCK2 D8
#define BUZZER_PIN D3
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

    // إعدادات ثابتة بدل فير بيز
    // serial = "ABC123";
    // weight = 100.0;
    // shelf_esp32_ip = "192.168.43.19";
    // shelf_total_weight = 2280.0;

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