#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <FirebaseESP8266.h>

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
float shelf_total_weight = 0;
float shelf_min_weight_diff = 0; // أقل فرق وزن حقيقي مستورد من فايربيز

// بيانات وزن محاكية
float simulated_weights[] = {
  5000, 5005, 4998, 5002,     // استقرار
  3990, 3985, 3988,           // أخذ 2
  3987, 3989,                 // استقرار
  4485                       // رجوع 1
};
const int num_weights = sizeof(simulated_weights) / sizeof(simulated_weights[0]);
int weight_index = 0;

float previous_weight = 0.0;

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

  if (Firebase.getFloat(fbdo, "/users/fj@fj,com/shelf_settings/min_weight_diff"))
    shelf_min_weight_diff = fbdo.floatData();

  // عرض إعدادات الرف
  Serial.println("📦 إعدادات الرف:");
  Serial.println("ESP32 IP: " + shelf_esp32_ip);
  Serial.print("الوزن الكلي: "); Serial.println(shelf_total_weight);
  Serial.print("أقل فرق وزن: "); Serial.println(shelf_min_weight_diff);
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
  Serial.print("الاسم: "); Serial.println(name);
  Serial.print("السعر: "); Serial.println(price);
  Serial.print("الرقم التسلسلي: "); Serial.println(serial);
  Serial.print("الرف: "); Serial.println(shelf);
  Serial.print("الوزن: "); Serial.println(weight);
  Serial.println("----------------------");

  // تعيين الوزن الابتدائي من البيانات
  previous_weight = simulated_weights[0];
  Serial.printf("⚖️ الوزن الابتدائي: %.2f g\n", previous_weight);
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
  if (weight_index < num_weights - 1)
  {
    float current_weight = simulated_weights[++weight_index];
    float diff = previous_weight - current_weight;

    Serial.printf("⚖️ الوزن الحالي: %.2f g | الفرق: %.2f g\n", current_weight, diff);

    if (abs(diff) >= shelf_min_weight_diff) // نستخدم القيمة من Firebase هنا
    {
      process_weight_change(diff);
      previous_weight = current_weight;
    }
  }

  delay(1500); // انتظار لمحاكاة التأخير بين القراءات
}
