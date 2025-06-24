#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <FirebaseESP8266.h>

const char *ssid = "Yosef";
const char *password = "28072004";
const char *host = "192.168.43.21"; // IP ESP32

#define API_KEY "AIzaSyCCi6Yvyfh7zPY_DqczkMcBUFdkmmI8xTA"
#define DATABASE_URL "https://smart-cart-f9c56-default-rtdb.firebaseio.com/"
#define USER_EMAIL "yoseffarhod@gmail.com"
#define USER_PASSWORD "y28072004"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

String name = "";
float price = 0;
String serial = "";
String shelf = "";

// بيانات وزن عشوائية لمحاكاة القراءة
float simulated_weights[] = {
    5000, 5005, 4998, 5002, // استقرار
    3990, 3985, 3988,       // نقص 1000 جم → أخذ 2
    3987, 3989,             // استقرار
    4485                    // رجع 1 → زاد 500
};
const int num_weights = sizeof(simulated_weights) / sizeof(simulated_weights[0]);
int weight_index = 0;

const float product_weight = 500.0; // جم
const float threshold = 30.0;       // أقل فرق نعتبره تغيير حقيقي

float previous_weight = 0.0;

void process_weight_change(float diff)
{
    int product_count = round(diff / product_weight);
    if (product_count > 0)
    {
        Serial.printf("✅ %d × %s taken\n", product_count, name.c_str());
        Serial.printf("📦 Barcode: %s\n\n", serial.c_str());
    }
    else if (product_count < 0)
    {
        Serial.printf("🔄 %d × %s returned\n", abs(product_count), name.c_str());
        Serial.printf("📦 Barcode: %s\n\n", serial.c_str());
    }
    // إرسال بيانات المنتج عند كل تغيير حقيقي (تشمل القراءة الفعلية)
    if (WiFi.status() == WL_CONNECTED)
    {
        WiFiClient client;
        HTTPClient http;
        String url = "http://" + String(host) +
                     "/update?serial=" + serial +
                     "&name=" + name +
                     "&price=" + String(price, 2) +
                     "&count=" + String(product_count) +
                     "&reading=" + String(previous_weight, 2); // إضافة القراءة الحالية
        http.begin(client, url);
        int httpCode = http.GET();
        if (httpCode > 0)
        {
            String response = http.getString();
            Serial.println("Server response: " + response);
        }
        else
        {
            Serial.println("Connection failed");
        }
        http.end();
    }
}

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

    previous_weight = simulated_weights[0];
    Serial.printf("Initial weight: %.2f g\n", previous_weight);
}

void loop()
{
    if (weight_index >= num_weights - 1)
        return; // انتهت البيانات

    float current_weight = simulated_weights[++weight_index];
    float diff = previous_weight - current_weight;
    Serial.printf("Current weight: %.2f g | Difference: %.2f g\n", current_weight, diff);

    if (abs(diff) >= threshold)
    {
        process_weight_change(diff);
        previous_weight = current_weight;
    }

    delay(1500); // محاكاة التأخير بين القراءات
}