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
        latestSerial = server.arg("serial");
        Serial.print("Received serial: ");
        Serial.println(latestSerial);
        server.send(200, "text/plain", "Serial received");
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