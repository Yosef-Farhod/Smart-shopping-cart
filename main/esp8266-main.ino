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

// بيانات المنتج
String name = "";
float price = 0;
String serial = "";
String shelf = "";
float weight = 0; // وزن المنتج المستورد من فايربيز

// إعدادات الرف
String shelf_esp32_ip = "";
float shelf_total_weight = 0;    //
float shelf_min_weight_diff = 0; // أقل فرق وزن حقيقي مستورد من فايربيز

// توصيلات الحساس الأول
#define DT1 D5
#define SCK1 D6

// توصيلات الحساس الثاني
#define DT2 D7
#define SCK2 D8

// توصيل البازر
#define BUZZER_PIN D3

// حد الوزن للتنبيه بالبـازر
#define BUZZER_THRESHOLD 10000.0 // بالجرام

HX711 scale1;
HX711 scale2;

float previous_weight = 0.0;      // تعريف المتغير هنا فقط مرة واحدة
String scanned_serial = "123456"; // تعريف المتغير هنا فقط مرة واحدة

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

  // عرض إعدادات الرف
  Serial.println("📦 إعدادات الرف:");
  Serial.println("ESP32 IP: " + shelf_esp32_ip);
  Serial.print("الوزن الموجود في تعريف الوزن");
  Serial.println(shelf_total_weight);
  Serial.println("----------------------");

  // جلب بيانات المنتج من Firebase
  if (Firebase.getString(fbdo, "/products/CyhYDpfJgNTcpQpMcfWK/name"))
    name = fbdo.stringData();

  if (Firebase.getFloat(fbdo, "/products/CyhYDpfJgNTcpQpMcfWK/price"))
    price = fbdo.floatData();

  if (Firebase.getString(fbdo, "/products/CyhYDpfJgNTcpQpMcfWK/serial"))
    serial = fbdo.stringData();

  if (Firebase.getString(fbdo, "/products/CyhYDpfJgNTcpQpMcfWK/shelf"))
    shelf = fbdo.stringData();

  if (Firebase.getFloat(fbdo, "/products/CyhYDpfJgNTcpQpMcfWK/weight"))
    weight = fbdo.floatData(); // نستخدم هذا كوزن المنتج

  // عرض بيانات المنتج
  Serial.println("📄 بيانات المنتج:");
  Serial.print("الاسم: ");
  Serial.println(name);
  Serial.print("السعر: ");
  Serial.println(price);
  Serial.print("الرقم التسلسلي: ");
  Serial.println(serial);
  Serial.print("الرف: ");
  Serial.println(shelf);
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
  int product_count = round(diff / weight); // نستخدم الوزن الفعلي من Firebase

  if (product_count == 0)
    return; // تجاهل التغييرات غير المؤثرة

  if (product_count > 0)
    Serial.printf("✅ %d × %s تم أخذها\n", product_count, name.c_str());
  else
    Serial.printf("🔄 %d × %s تم إرجاعها\n", abs(product_count), name.c_str());

  Serial.printf("📦 Barcode: %s\n\n", serial.c_str());

  // إرسال البيانات إلى الرف عبر HTTP
  if (WiFi.status() == WL_CONNECTED)
  {
    WiFiClient client;
    HTTPClient http;

    String url = "http://" + shelf_esp32_ip +
                 "/update?serial=" + serial +
                 "&name=" + name +
                 "&price=" + String(price, 2) +
                 "&count=" + String(product_count) +
                 "&reading=" + String(previous_weight, 2) +
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
    float diff = last_sent_weight - totalWeight;
    if (abs(diff) >= 30) // استخدام الحد الأدنى من الوزن
    {
      int product_count = round(diff / weight);
      if (product_count != 0)
      {
        if (product_count > 0)
          Serial.printf("✅ %d × %s تم أخذها\n", product_count, name.c_str());
        else
          Serial.printf("🔄 %d × %s تم إرجاعها\n", abs(product_count), name.c_str());
        Serial.printf("📦 Barcode: %s\n\n", serial.c_str());

        // إرسال البيانات إلى الرف عبر HTTP
        if (WiFi.status() == WL_CONNECTED)
        {
          WiFiClient client;
          HTTPClient http;
          String url = "http://" + shelf_esp32_ip +
                       "/update?serial=" + serial +
                       "&name=" + name +
                       "&price=" + String(price, 2) +
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

  // مقارنة الرقم التسلسلي المقروء مع رقم المنتج من فايربيز
  if (scanned_serial == serial)
  {
    Serial.println("✅ تم عمل اسكان للمنتج بنجاح (Serial Match)");
    // إرسال رد للرف (ESP32) أن الاسكان تم بنجاح
    if (WiFi.status() == WL_CONNECTED)
    {
      WiFiClient client;
      HTTPClient http;
      String url = "http://" + shelf_esp32_ip + "/update?scan=ok&serial=" + scanned_serial;
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
    // بعد أول مطابقة، امسح المتغير حتى لا تتكرر العملية
    scanned_serial = "";
  }

  delay(3000);
}
