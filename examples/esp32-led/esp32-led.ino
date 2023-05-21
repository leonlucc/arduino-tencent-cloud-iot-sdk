#include <WiFi.h>
#include <TencentCloudIoTSDK.h>

#define WIFI_SSID "xxxxxx"
#define WIFI_PWD "xxxxxxxx"
#define PRODUCT_ID "xxxxx"
#define DEVICE_NAME "xxxxx"
#define DEVICE_SECRET "xxxxxxxxxxx"

WiFiClient espClient;
unsigned long lastMsMain = 0;
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

    TencentCloudIoTSDK::begin(espClient, PRODUCT_ID, DEVICE_NAME, DEVICE_SECRET);

    // 绑定一个设备属性回调，当远程修改此属性，会触发powerCallback。power_switch 是在设备产品中定义的物模型属性的标识符
    TencentCloudIoTSDK::bindProperty("power_switch", powerSwitchCallback);

    // 绑定一个行为回调，当远程调用行为，会触发alarmCallback。alarm是在设备产品中定义的物模型行为的标识符
    TencentCloudIoTSDK::bindAction("alarm", alarmCallback);
}

void loop()
{
    TencentCloudIoTSDK::loop();
    if (millis() - lastMsMain >= 5000)
    {
        lastMsMain = millis();
        // 发送设备状态事件
        sprintf(deviceStatusParam, "{\"running_state\": %d}", 0);
        TencentCloudIoTSDK::sendEvent("status_report", deviceStatusParam);
    }
    // 发送模型属性
    TencentCloudIoTSDK::sendProperty("color", 0);
}

// 远程警报调用的回调函数
TencentCloudIoTSDK::Result alarmCallback(JsonVariant param)
{
    int alarm = param["alarm"];
    // Do other stuff...
    return result; // 返回正常结果， code=0
}

// 电源属性修改的回调函数
TencentCloudIoTSDK::Result powerSwitchCallback(JsonVariant param)
{
    int PowerSwitch = param["power_switch"];
    if (PowerSwitch == 1) // 电源开关打开
    {
        // Do other stuff...
    }
    else // 电源开关关闭
    {
        // Do other stuff...
    }
    return result; // 返回正常结果， code=0
}
