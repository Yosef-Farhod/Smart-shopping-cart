#include <WiFi.h>
#include <WebServer.h>
#include <HX711.h>
#include <HTTPClient.h> // أضف هذا السطر إذا لم يكن موجوداً

// تعيين بنات الاتصال التسلسلي مع GM65
#define RXD2 16 // توصيل RX من GM65
#define TXD2 17 // توصيل TX من GM65

// توصيلات حساس الوزن
#define SCALE_DT 18  // مثال: GPIO18
#define SCALE_SCK 19 // مثال: GPIO19

#define BUZZER_PIN 21 // مثال: GPIO21

const char *ssid = "Yosef";
const char *password = "28072004";

WebServer server(80);

HX711 scale; // <-- احذف أي حرف زائد بعد التعريف

// متغيرات عامة لتخزين آخر البيانات المستلمة
String latestSerial = "";
String latestCount = "";
String latestReading = "";
float latestWeight = 0.0;
int products_to_scan = 0;
int scanned_count = 0;
unsigned long scan_start_time = 0;
const unsigned long SCAN_TIMEOUT = 20000; // 20 ثانية
bool waiting_for_scan = false;
bool buzzer_on = false;

void handleUpdate()
{
    Serial.println("📥 Received request on /update");

    if (server.hasArg("serial"))
    {
        // تحديث القيم من الطلب
        latestSerial = server.arg("serial");
        latestCount = server.hasArg("count") ? server.arg("count") : "";
        latestReading = server.hasArg("reading") ? server.arg("reading") : "";
        latestWeight = server.hasArg("weight") ? server.arg("weight").toFloat() : 0.0;

        // عرض البيانات على الـ Serial Monitor
        Serial.println("✅ تم استلام البيانات:");
        Serial.println("Serial: " + latestSerial);
        Serial.println("Count: " + latestCount);
        Serial.println("Reading: " + latestReading);
        Serial.print("Weight: ");
        Serial.println(latestWeight);

        // ابدأ الانتظار للاسكان إذا count > 0
        products_to_scan = latestCount.toInt();
        scanned_count = 0;
        if (products_to_scan > 0)
        {
            waiting_for_scan = true;
            scan_start_time = millis();
            buzzer_on = false;
        }

        server.send(200, "text/plain", "Data received");
    }
    else if (server.hasArg("scan") && server.arg("scan") == "ok" && server.hasArg("serial"))
    {
        // استقبل اسكان جديد
        if (waiting_for_scan && scanned_count < products_to_scan)
        {
            scanned_count++;
            Serial.printf("تم اسكان منتج (%d/%d)\n", scanned_count, products_to_scan);
            if (scanned_count >= products_to_scan)
            {
                waiting_for_scan = false;
                buzzer_on = false;
                digitalWrite(BUZZER_PIN, LOW);
                Serial.println("✅ تم اسكان كل المنتجات المطلوبة.");
            }
        }
        server.send(200, "text/plain", "Scan OK received");
    }
    else
    {
        Serial.println("❌ Missing serial in request");
        server.send(400, "text/plain", "Missing serial");
    }
}

// دالة اختبار (اختياري)
void handleRoot()
{
    server.send(200, "text/plain", "ESP32 Receiver is running");
}

void setup()
{
    Serial.begin(115200);

    // الاتصال بالواي فاي
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\n✅ Connected to WiFi");
    Serial.print("🌐 ESP32 IP Address: ");
    Serial.println(WiFi.localIP());

    // ربط المسارات
    server.on("/", handleRoot);
    server.on("/update", handleUpdate);
    server.begin();
    Serial.println("🚀 Server started");

    // تهيئة الاسكانر
    Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);
    Serial.println("Barcode scanner ready...");

    // تهيئة حساس الوزن
    scale.begin(SCALE_DT, SCALE_SCK);
    scale.set_scale(350); // تحتاج للمعايرة الفعلية حسب حساسك
    scale.tare();
    Serial.println("Weight scale ready...");

    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);
}

void loop()
{
    server.handleClient();

    // التحقق من وصول بيانات من الماسح
    if (Serial2.available())
    {
        String barcode = "";
        while (Serial2.available())
        {
            char c = Serial2.read();
            barcode += c;
            delay(5); // لتفادي فقد البيانات
        }
        Serial.print("Scanned barcode: ");
        Serial.println(barcode);

        // إرسال scan=ok تلقائياً للرف عند قراءة الباركود
        WiFiClient client;
        HTTPClient http;
        // يجب أن يكون لديك IP الرف هنا، استخدم latestSerial إذا كان هو IP أو خزنه في متغير منفصل
        String shelf_ip = latestSerial; // عدل حسب الحاجة
        String url = "http://" + shelf_ip + "/update?scan=ok&serial=" + barcode;
        http.begin(client, url);
        int httpCode = http.GET();
        if (httpCode > 0)
        {
            String response = http.getString();
            Serial.println("📡 Scan response sent: " + response);
        }
        else
        {
            Serial.println("❌ فشل في إرسال رد الاسكان");
        }
        http.end();
    }

    // مراقبة الوزن في السلة
    static float last_weight = 0.0;
    float current_weight = scale.get_units(10); // متوسط 10 قراءات

    // منطق البازر الجديد
    static bool buzzer_error_active = false;
    static unsigned long buzzer_error_start = 0;

    if (!isnan(current_weight) && current_weight != 0.0)
    {
        Serial.print("وزن السلة: ");
        Serial.println(current_weight);

        if (waiting_for_scan && products_to_scan > 0)
        {
            float expected_weight = products_to_scan * (latestWeight > 0 ? latestWeight : 100);
            float weight_diff = current_weight - last_weight;

            // إذا زاد الوزن ولم يتم عمل اسكانر لأي منتج (scanned_count == 0)، شغّل البازر لمدة 5 ثواني فقط
            if ((current_weight - last_weight) > 30 && scanned_count == 0)
            {
                if (!buzzer_error_active)
                {
                    buzzer_error_active = true;
                    buzzer_error_start = millis();
                    digitalWrite(BUZZER_PIN, HIGH);
                }
            }

            // إذا تم اسكان كل المنتجات، أوقف البازر
            if (scanned_count >= products_to_scan)
            {
                buzzer_error_active = false;
                digitalWrite(BUZZER_PIN, LOW);
                waiting_for_scan = false;
            }
        }
        else
        {
            buzzer_error_active = false;
            digitalWrite(BUZZER_PIN, LOW);
        }
    }
    else
    {
        Serial.println("⚠️ قراءة الحساس غير صالحة أو صفرية!");
        buzzer_error_active = false;
        digitalWrite(BUZZER_PIN, LOW);
    }

    // إطفاء البازر بعد 5 ثواني من الخطأ
    if (buzzer_error_active && (millis() - buzzer_error_start >= 5000))
    {
        buzzer_error_active = false;
        digitalWrite(BUZZER_PIN, LOW);
    }

    last_weight = current_weight;

    delay(1000);
}