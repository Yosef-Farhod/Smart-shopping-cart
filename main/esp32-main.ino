#include <WiFi.h>
#include <WebServer.h>

const char *ssid = "Yosef";
const char *password = "28072004";

WebServer server(80);

// متغيرات عامة لتخزين آخر البيانات المستلمة
String latestSerial = "";
String latestCount = "";
String latestReading = "";

// أضف متغير جديد لمحاكاة الرقم التسلسلي المقروء من الاسكانر
String scanned_serial = "123456"; // غيّر القيمة للاختبار

void handleUpdate()
{
    Serial.println("📥 Received request on /update");

    if (server.hasArg("serial"))
    {
        // تحديث القيم من الطلب
        latestSerial = server.arg("serial");
        latestCount = server.hasArg("count") ? server.arg("count") : "";
        latestReading = server.hasArg("reading") ? server.arg("reading") : "";

        // عرض البيانات على الـ Serial Monitor
        Serial.println("✅ تم استلام البيانات:");
        Serial.println("Serial: " + latestSerial);
        Serial.println("Count: " + latestCount);
        Serial.println("Reading: " + latestReading);

        // مقارنة الرقم التسلسلي المقروء مع الرقم المستلم
        if (scanned_serial == latestSerial)
        {
            Serial.println("🔔 تم عمل اسكان للمنتج بنجاح (Serial Match)");
            // يمكنك هنا تنفيذ أي منطق إضافي عند المطابقة
        }

        server.send(200, "text/plain", "Data received");
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
}

void loop()
{
    server.handleClient();
}

// الكود الحالي للسلة (ESP32) بالفعل يستقبل كل البيانات ويراقب الرقم التسلسلي ويطابقه مع المتغير scanned_serial.
// لا تحتاج لتعديل إضافي إذا كنت تريد فقط استقبال البيانات من الرف (ESP8266) والتحقق من الرقم التسلسلي.
// إذا أردت إضافة منطق إضافي عند المطابقة أو إرسال رد، يمكنك ذلك هنا.
// الكود الحالي للسلة (ESP32) بالفعل يستقبل كل البيانات ويراقب الرقم التسلسلي ويطابقه مع المتغير scanned_serial.
// لا تحتاج لتعديل إضافي إذا كنت تريد فقط استقبال البيانات من الرف (ESP8266) والتحقق من الرقم التسلسلي.
// إذا أردت إضافة منطق إضافي عند المطابقة أو إرسال رد، يمكنك ذلك هنا.
// الكود الحالي للسلة (ESP32) بالفعل يستقبل كل البيانات ويراقب الرقم التسلسلي ويطابقه مع المتغير scanned_serial.
// لا تحتاج لتعديل إضافي إذا كنت تريد فقط استقبال البيانات من الرف (ESP8266) والتحقق من الرقم التسلسلي.
// إذا أردت إضافة منطق إضافي عند المطابقة أو إرسال رد، يمكنك ذلك هنا.
