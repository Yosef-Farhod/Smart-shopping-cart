#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <FirebaseESP8266.h>
#include <HX711.h>

// بيانات WiFi
const char *ssid = "Yosef";
const char *password = "28072004";

// إعدادات Firebase
#define API_KEY "AIzaSyCCi6Yvyfh7zPY_DqczkMcBUFdkmmI8xTA"
#define DATABASE_URL "https://smart-cart-f9c56-default-rtdb.firebaseio.com/"
#define USER_EMAIL "yoseffarhod@gmail.com"
#define USER_PASSWORD "y28072004"

// كائنات Firebase
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

int product_count = round(diff / weight);

// بيانات المنتج
String serial = "";
float weight = 0; // وزن المنتج المستورد من فايربيز

// إعدادات الرف
String shelf_esp32_ip = "";
float shelf_total_weight = 0; // ضبط الوزن للحساس

// توصيلات الحساس الأول
#define DT1 D5
#define SCK1 D6

// توصيلات الحساس الثاني
#define DT2 D7
#define SCK2 D8

// توصيل البازر
#define BUZZER_PIN D3

// حد الوزن للتنبيه بالبـازر
#define BUZZER_THRESHOLD 4900 // بالجرام

HX711 scale1;
HX711 scale2;

float previous_weight = 0.0; // تعريف المتغير هنا فقط مرة واحدة
// لم يعد هناك حاجة لمتغير scanned_serial، سنستقبل الرقم التسلسلي من السلة عبر HTTP

// متغيرات لمتابعة التأكيد من السلة
bool waiting_for_scan_ok = false;
unsigned long scan_request_time = 0;      // <-- كان معرف غلط
const unsigned long SCAN_TIMEOUT = 20000; // 10 ثواني

void setup()
{
  Serial.begin(115200);

  // الاتصال بالواي فاي
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

  // إعداد البازر
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // تهيئة الحساسات
  scale1.begin(DT1, SCK1);
  scale2.begin(DT2, SCK2);
  scale1.set_scale(shelf_total_weight);
  scale2.set_scale(shelf_total_weight);
  scale1.tare();
  scale2.tare();

  Serial.println("تم تهيئة الحساسين وتصفير الوزن.");

  // إعدادات Firebase
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  Firebase.reconnectNetwork(true);
  fbdo.setBSSLBufferSize(1024, 1024);
  fbdo.setResponseSize(1024);
  Firebase.begin(&config, &auth);
  Firebase.setDoubleDigits(5);
  config.timeout.serverResponse = 10 * 1000;

  // جلب إعدادات الرف
  if (Firebase.getString(fbdo, "/users/fj@fj,com/shelf_settings/esp32_ip"))
    shelf_esp32_ip = fbdo.stringData();
  else
    Serial.println("❌ فشل في جلب IP: " + fbdo.errorReason());
  shelf_esp32_ip = "192.168.43.21"; // تعيين IP افتراضي في حالة الفشل

  if (Firebase.getFloat(fbdo, "/users/fj@fj,com/shelf_settings/total_weight"))
    shelf_total_weight = fbdo.floatData();

  // جلب بيانات المنتج من Firebase
  if (Firebase.getString(fbdo, "/products/CyhYDpfJgNTcpQpMcfWK/serial"))
    serial = fbdo.stringData();

  if (Firebase.getFloat(fbdo, "/products/CyhYDpfJgNTcpQpMcfWK/weight"))
    weight = fbdo.floatData(); // نستخدم هذا كوزن المنتج

  // عرض إعدادات الرف
  Serial.println("📦 إعدادات الرف:");
  Serial.println("ESP32 IP: " + shelf_esp32_ip);
  Serial.print("الوزن الموجود في تعريف الوزن");
  Serial.println(shelf_total_weight);
  Serial.println("----------------------");

  // عرض بيانات المنتج
  Serial.println("📄 بيانات المنتج:");
  Serial.print("الرقم التسلسلي: ");
  Serial.println(serial);
  Serial.print("الوزن: ");
  Serial.println(weight);
  Serial.println("----------------------");

  // عند بدء التشغيل، عيّن previous_weight إلى الوزن الحالي من الحساسين (أو صفر)
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

// دالة لمعالجة تغير الوزن الفعلي
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

  // إرسال البيانات إلى الرف عبر HTTP
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
      // بعد إرسال الطلب، فعّل وضع الانتظار للتأكيد فقط إذا تم أخذ منتج
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

// الحلقة الرئيسية لمراقبة الوزن
void loop()
{
  // قراءة الوزن من الحساسين إذا كانا جاهزين
  float totalWeight = 0;
  float weight1 = 0, weight2 = 0;

  if (scale1.is_ready())
    weight1 = scale1.get_units(10); // يقرأ 10 مرات ويحسب المتوسط
  if (scale2.is_ready())
    weight2 = scale2.get_units(10); // يقرأ 10 مرات ويحسب المتوسط

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

    // معالجة تغير الوزن وإرسال البيانات إذا تجاوز الفرق الحد الأدنى
    static float last_sent_weight = 0;
    float diff = totalWeight - last_sent_weight;
    if (abs(diff) >= 30) // استخدام الحد الأدنى من الوزن
    {
      int product_count = round(diff / weight);
      if (product_count != 0)
      {
        if (product_count > 0)
          Serial.printf("✅ %d تم أخذها\n", product_count);
        else
          Serial.printf("🔄 %d تم إرجاعها\n", abs(product_count));
        Serial.printf("📦 Barcode: %s\n\n", serial.c_str());

        // إرسال البيانات إلى الرف عبر HTTP
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
  else
  {
    Serial.println("خطأ: أحد الحساسات غير جاهز.");
  }

  // استقبال تأكيد الاسكان من السلة عبر HTTP (scan=ok)
  // هذا يتم تلقائياً في دالة handleUpdate في كود السلة (ESP32)
  // في كود الرف (ESP8266)، لا داعي لمقارنة متغير محلي مثل scanned_serial مع serial
  // لأن التأكيد يصل عبر HTTP ويوقف البازر في منطق الانتظار بالفعل

  // منطق الانتظار وتشغيل البازر إذا لم يصل التأكيد خلال المهلة
  if (waiting_for_scan_ok)
  {
    if (millis() - scan_request_time > SCAN_TIMEOUT)
    {
      digitalWrite(BUZZER_PIN, HIGH); // شغّل البازر
    }
    else
    {
      digitalWrite(BUZZER_PIN, LOW); // لا تشغّل البازر أثناء الانتظار
    }
  }
  else
  {
    digitalWrite(BUZZER_PIN, LOW); // تأكد أن البازر مطفأ إذا لا يوجد انتظار
  }

  delay(3000);
}