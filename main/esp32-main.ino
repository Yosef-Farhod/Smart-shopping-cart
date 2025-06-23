#include <WiFi.h>
#include <WebServer.h>

const char *ssid = "Yosef";
const char *password = "28072004";

WebServer server(80);

float latestWeight = 0.0;

void handleUpdate()
{
  Serial.println("Received request on /update");
  if (server.hasArg("weight"))
  {
    latestWeight = server.arg("weight").toFloat();
    Serial.print("Received weight: ");
    Serial.println(latestWeight);
    server.send(200, "text/plain", "Weight received");
  }
  else
  {
    server.send(400, "text/plain", "Missing weight");
  }
}

void setup()
{
  Serial.begin(115200);
  WiFi.begin(ssid, password); // اتصال كعميل وليس كنقطة وصول
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