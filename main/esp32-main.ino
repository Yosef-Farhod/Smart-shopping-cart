#include <WiFi.h>
#include <WebServer.h>

const char *ssid = "Yosef";
const char *password = "28072004";

WebServer server(80);

String latestSerial = ""; // متغير لحفظ الرقم التسلسلي

void handleUpdate()
{
    Serial.println("Received request on /update");
    if (server.hasArg("serial"))
    {
        String latestSerial = server.arg("serial");
        String latestName = server.hasArg("name") ? server.arg("name") : "";
        String latestPrice = server.hasArg("price") ? server.arg("price") : "";
        String latestCount = server.hasArg("count") ? server.arg("count") : "";
        String latestReading = server.hasArg("reading") ? server.arg("reading") : "";

        Serial.print("Received serial: ");
        Serial.println(latestSerial);
        Serial.print("Name: ");
        Serial.println(latestName);
        Serial.print("Price: ");
        Serial.println(latestPrice);
        Serial.print("Count: ");
        Serial.println(latestCount);
        Serial.print("Reading: ");
        Serial.println(latestReading);

        server.send(200, "text/plain", "Data received");
    }
    else
    {
        server.send(400, "text/plain", "Missing serial");
    }
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
    Serial.println("\nConnected to WiFi");
    Serial.print("IP Addres ESP32 : ");
    Serial.println(WiFi.localIP());

    server.on("/update", handleUpdate);
    server.begin();
}

void loop()
{
    server.handleClient();
}