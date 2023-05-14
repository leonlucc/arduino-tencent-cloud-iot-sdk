#include <WiFi.h>
#include <TencentCloudIoTSDK.h>

#define WIFI_SSID "xxxxxx"
#define WIFI_PWD "xxxxxxxx"
#define PRODUCT_KEY "xxxxx"
#define DEVICE_NAME "xxxxx"
#define DEVICE_SECRET "xxxxxxxxxxx"

WiFiClient espClient;
char deviceStatusParam[64];
TencentCloudIoTSDK::Result result;

void wifiInit(const char *ssid, const char *passphrase)
{
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, passphrase);
    WiFi.setAutoConnect(true);
    WiFi.setAutoReconnect(true);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(1000);
        Serial.println("WiFi not Connect");
    }
    Serial.println("Connected to AP");
}

void setup()
{
    Serial.begin(115200);

    wifiInit(WIFI_SSID, WIFI_PWD);

    TencentCloudIoTSDK::begin(espClient, PRODUCT_KEY, DEVICE_NAME, DEVICE_SECRET);
}

void loop()
{
    TencentCloudIoTSDK::loop();
    TencentCloudIoTSDK::sendProperty("temperature", 25);
    TencentCloudIoTSDK::sendProperty("humidity", 60);
}

