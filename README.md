# TencentCloudIoTSDK 腾讯云物联网平台开发库
腾讯云物联网平台快速对接开发库，封装了物模型协议，支持属性、事件和行动的MQTT消息上传下发。

## 简介
为简化使用Arduino Framwork的开发者连接腾讯云物联网平台时的基础工作，本项目在PubSubClient等开源库等基础上封装了腾讯云物模型协议，使用时只要提供从腾讯云物联网平台上获得设备身份认证信息（ProductID、DeviceName、DeviceSecret）就可以迅速完成MQTT接入，实现物模型属性、事件和行动的MQTT消息上传下发。

## 依赖项
需要安装如下依赖库 
- [PubSubClient](https://github.com/knolleary/pubsubclient)
- [ArduinoJson](https://arduinojson.org/)
- [Crypto](https://rweather.github.io/arduinolibs/index.html)
- [Base64_Codec](https://github.com/dojyorin/arduino_base64)

## 安装
打开Arduino IDE -> 项目 -> 加载库 -> 管理库 -> 搜索 "TencentCloudIoTSDK"

## 使用示例

```c++
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
```
更多示例请参见[这里](https://github.com/leonlucc/arduino-tencent-cloud-iot-sdk//tree/main/examples)

## API
### 接口 
```c++
/**
 * 初始化并接入腾讯云物联网平台
 * 腾讯云mqtt接入文档：https://cloud.tencent.com/document/product/634/32546
 * 腾讯云物模型协议文档： https://cloud.tencent.com/document/product/1081/34916
 * @param client 客户端
 * @param productId 产品ID
 * @param deviceName 设备名称
 * @param deviceSecret 设备密钥
 * @param expiry 签名的过期时间,从纪元1970年1月1日 00:00:00 UTC 时间至今秒数，默认为2030-12-31 23:59:59
 */
void begin(Client &client, const char *productId,
            const char *deviceName,
            const char *deviceSecret,
            unsigned long expiry = DEFAULT_EXPIRY);

/**
 * 检查连接和定时发送信息，在主程序loop中调用
 */
void loop();

/**
 * 直接发送json属性
 * 无缓存，需自行控制发送频率
 * @param params 字符串形式的json属性参数，例如 {"${propId}":"${value}"}，支持多个属性，
 */
void sendProperties(const char *params);
/**
 * 定时发送float格式属性
 * 有缓存，多个属性一起发送，默认间隔时间5s
 * @param propId 属性的标识符
 * @param number 属性的值
 */
void sendProperty(char *propId, float number);
/**
 * 定时发送int格式属性
 * 有缓存，多个属性一起发送，默认间隔时间5s
 * @param propId 物模型的属性标识符
 * @param number 属性的值
 */
void sendProperty(char *propId, int number);
/**
 * 定时发送double格式属性
 * 有缓存，多个属性一起发送，默认间隔时间5s
 * @param propId 物模型的属性标识符
 * @param number 属性的值
 */
void sendProperty(char *propId, double number);
/**
 * 定时发送string格式属性
 * 有缓存，多个属性一起发送，默认间隔时间5s
 * @param propId 物模型的属性标识符
 * @param text 属性的值
 */
void sendProperty(char *propId, char *text);

/**
 * 发送事件到云平台
 * @param eventId 物模型的事件标识符
 * @param param 字符串形式的json参数，例如 {"${key}":"${value}"}，默认为空
 * @param eventType 事件类型，默认为info
 */
void sendEvent(const char *eventId, const char *params = "{}", EventType eventType = info);

/**
 * 绑定属性回调，云服务下发的属性包含此 propId 会进入回调，用于监听特定属性的下发
 * @param propId 物模型的属性标识符
 * @param CallbackFun 回调函数
 */
int bindProperty(char *propId, CallbackFun callback);

/**
 * 绑定行为回调，云服务下发的特定行为会进入回调，用于执行特定行为
 * @param eventId 物模型的事件标识符
 * @param CallbackFun 回调函数
 */
int bindAction(char *actionId, CallbackFun callback);
```
### 类型 
```c++
// 回调函数处理结果
struct Result
{
    int code = 0;
    char *status = "SUCC";
};

// 回调函数的函数指针, 返回处理结果。
typedef Result (*CallbackFun)(JsonVariant params);

// 事件类型
enum EventType
{
    info,
    alert,
    fault
};
```
## 使用限制和说明

- 本SDK不封装WIFI连接的代码，需先建立连接，然后将client传入
- PubSubClient中的MQTT_MAX_PACKET_SIZE默认已设定为 1024
- PubSubClient中的MQTT_KEEPALIVE默认已设定为60
- 默认每10s检测一次MQTT连接状态，若发现连接失败会尝试自动重连
- 上传数据时直接发送属性（无缓存，需自行控制发送频率）和定时发送属性（有缓存，多个属性一起发送，默认间隔时间5s）两种模式

## 适用硬件
本SDK基于PubSubClient底层库开发，兼容列表与PubSubClient相同。目前已验证的芯片包括:
- ESP8266
- ESP32-C3

其他经验证的芯片和开发板信息将逐步更新到上面列表中

## 感谢
- 本项目参考了[AliyunIoTSDK](https://github.com/aoao-eth/arduino-aliyun-iot-sdk)

## License
MIT
