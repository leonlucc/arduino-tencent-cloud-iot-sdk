
#include "TencentCloudIoTSDK.h"
#include <SHA256.h>
#include <arduino_base64.hpp>

#define MQTT_PORT 1883
#define MQTT_BUFFER_SIZE 1024
#define MQTT_KEEP_ALIVE 60

#define SHA256_HMAC_SIZE 32
#define CALLBACK_MAP_SIZE 20
#define LOOP_CHECK_INTERVAL 10000
#define RETRY_CRASH_COUNT 5
#define PROP_BUFFER_SIZE 10
#define PROP_BUFFER_CHECK_INTERVA 5000

// MQTT域名和Topic
#define MQTT_DOMAIN_FORMAT "%s.iotcloud.tencentdevices.com"
#define MQTT_TOPIC_PROP_UP_FORMAT "$thing/up/property/%s/%s"
#define MQTT_TOPIC_PROP_DOWN_FORMAT "$thing/down/property/%s/%s"
#define MQTT_TOPIC_EVENT_UP_FORMAT "$thing/up/event/%s/%s"
#define MQTT_TOPIC_EVENT_DOWN_FORMAT "$thing/down/event/%s/%s"
#define MQTT_TOPIC_ACTION_UP_FORMAT "$thing/up/action/%s/%s"
#define MQTT_TOPIC_ACTION_DOWN_FORMAT "$thing/down/action/%s/%s"

// 物模型参数
#define THING_MODEL_PROP_BODY_FORMAT "{\"method\":\"report\",\"clientToken\":\"%d\",\"params\":%s}"
#define THING_MODEL_EVENT_BODY_FORMAT "{\"method\":\"event_post\",\"clientToken\":\"%d\",\"version\": \"1.0\",\"eventId\": \"%s\",\"type\": \"%s\",\"params\":%s}"
#define THING_MODEL_CTRL_REPLY_BODY_FORMAT "{\"method\":\"control_reply\",\"clientToken\":\"%s\",\"code\": %d,\"status\": \"%s\"}"
#define THING_MODEL_ACTION_REPLY_BODY_FORMAT "{\"method\":\"action_reply\",\"clientToken\":\"%s\",\"code\": %d,\"status\": \"%s\",\"response\": {}}"

namespace
{
    // 物模型topic
    char thingModelTopicPropUp[128];
    char thingModelTopicPropDown[128];
    char thingModelTopicEventUp[128];
    char thingModelTopicEventDown[128];
    char thingModelTopicActionUp[128];
    char thingModelTopicActionDown[128];

    // mqtt连接信息
    char mqttDomain[128];
    char mqttClientId[128];
    char mqttUsername[128];
    char mqttPwd[256];
    
    // mqtt客户端
    PubSubClient *pubSubClient;
    unsigned long lastConnCheckTime = 0;
    unsigned long retryConnCount = 0;

    // 回调函数映射表
    typedef struct CallbackMap
    {
        char *id; // 标识符，全局唯一
        TencentCloudIoTSDK::CallbackFun callback;
    } CallbackMap;
    CallbackMap callbackMap[CALLBACK_MAP_SIZE]; // 默认最多绑定20个回调

    // 设备属性上传缓存
    struct DeviceProperty
    {
        char *key;
        String value;
    };
    DeviceProperty propertyMessageBuffer[PROP_BUFFER_SIZE];
    int propertyMessageBufferLength = 0;
    unsigned long lastSendBufferTime = 0;

    void (*resetFunc)(void) = 0; // 制造重启命令

    // HMAC-SHA256基于设备密钥生成摘要签名
    String hmac256(const String &signcontent, const String &ds)
    {
        byte hashCode[SHA256_HMAC_SIZE];
        SHA256 sha256;

        const char *key = ds.c_str();
        size_t keySize = ds.length();

        sha256.resetHMAC(key, keySize);
        sha256.update((const byte *)signcontent.c_str(), signcontent.length());
        sha256.finalizeHMAC(key, keySize, hashCode, sizeof(hashCode));

        String sign = "";
        for (byte i = 0; i < SHA256_HMAC_SIZE; ++i)
        {
            sign += "0123456789ABCDEF"[hashCode[i] >> 4];
            sign += "0123456789ABCDEF"[hashCode[i] & 0xf];
        }

        return sign;
    }

    void sendActionReply(const char *clientToken, int code, char *status)
    {
        char jsonBuf[1024];
        sprintf(jsonBuf, THING_MODEL_ACTION_REPLY_BODY_FORMAT, clientToken, code, status);
        Serial.println(jsonBuf);
        boolean d = pubSubClient->publish(thingModelTopicActionUp, jsonBuf);
        Serial.print("publish:0 成功:");
        Serial.println(d);
    }

    // 所有云服务的回调都会首先进入这里，例如属性下发
    void callback(char *topic, byte *payload, unsigned int length)
    {
        Serial.print("Message arrived in topic: ");
        Serial.println(topic);
        Serial.print("Message:");
        payload[length] = '\0';
        Serial.println((char *)payload);

        StaticJsonDocument<200> doc;
        DeserializationError error = deserializeJson(doc, payload); // 反序列化JSON数据

        if (!error) // 检查反序列化是否成功
        {
            JsonVariant json = doc.as<JsonVariant>();
            if (strstr(topic, thingModelTopicPropDown))
            {
                for (int i = 0; i < CALLBACK_MAP_SIZE; i++)
                {
                    if (callbackMap[i].id)
                    {
                        bool hasKey = json["params"].containsKey(callbackMap[i].id);
                        if (hasKey)
                        {
                            callbackMap[i].callback(json["params"]);
                        }
                    }
                }
            }
            else if (strstr(topic, thingModelTopicActionDown))
            {
                for (int i = 0; i < CALLBACK_MAP_SIZE; i++)
                {
                    if (callbackMap[i].id && strcmp(json["actionId"], callbackMap[i].id) == 0)
                    {
                        TencentCloudIoTSDK::Result result = callbackMap[i].callback(json["params"]);
                        String clientToken = json["clientToken"];
                        sendActionReply(clientToken.c_str(), result.code, result.status);
                    }
                }
            }
        }
    }

    void mqttReconnect()
    {
        if (pubSubClient != NULL)
        {
            if (!pubSubClient->connected())
            {
                Serial.println("Connecting to MQTT Server ...");
                if (pubSubClient->connect(mqttClientId, mqttUsername, mqttPwd))
                {
                    Serial.println("MQTT Connected!");
                }
                else
                {
                    Serial.print("MQTT Connect error:");
                    Serial.println(pubSubClient->state());
                    retryConnCount++;
                    if (retryConnCount > RETRY_CRASH_COUNT)
                    {
                        resetFunc();
                    }
                }
            }
            else
            {
                Serial.println("state is connected");
                retryConnCount = 0;
            }
        }
    }

    // 发送 buffer 数据
    void sendBuffer()
    {
        int i;
        String buffer;
        for (i = 0; i < propertyMessageBufferLength; i++)
        {
            buffer += "\"" + String(propertyMessageBuffer[i].key) + "\":" + propertyMessageBuffer[i].value + ",";
        }
        buffer = "{" + buffer.substring(0, buffer.length() - 1) + "}";
        TencentCloudIoTSDK::sendProperties(buffer.c_str());
    }

    // 检查是否有数据需要发送
    void messageBufferCheck()
    {
        if (propertyMessageBufferLength > 0)
        {
            unsigned long nowMS = millis();
            // 按照默认5s间隔发送一次数据
            if (nowMS - lastSendBufferTime > PROP_BUFFER_CHECK_INTERVA)
            {
                sendBuffer();
                lastSendBufferTime = nowMS;
            }
        }
    }

    void addMessageToBuffer(char *key, String value)
    {
        int targetIndex = -1; //插入目标位置
        for (int i = 0; i < propertyMessageBufferLength; i++)
        {
            if (strcmp(propertyMessageBuffer[i].key, key) == 0) // 如果数组中已有相同key则覆盖
            {
                targetIndex = i;
                break;
            }
        }
        if (targetIndex == -1) //插入数组末尾
        {
            // 数组已满时无法插入新的属性
            if (propertyMessageBufferLength == PROP_BUFFER_SIZE) 
            {
                return;
            }
            targetIndex = propertyMessageBufferLength; 
            propertyMessageBufferLength++;
        }
        propertyMessageBuffer[targetIndex].key = key;
        propertyMessageBuffer[targetIndex].value = value;
    }

}

void TencentCloudIoTSDK::begin(Client &client, const char *productId,
                               const char *deviceName,
                               const char *deviceSecret,
                               unsigned long expiry)
{
    // 初始化pubSubClient
    pubSubClient = new PubSubClient(client);
    pubSubClient->setBufferSize(MQTT_BUFFER_SIZE);
    pubSubClient->setKeepAlive(MQTT_KEEP_ALIVE);
    
    sprintf(mqttDomain, MQTT_DOMAIN_FORMAT, productId);
    sprintf(thingModelTopicPropUp, "$thing/up/property/%s/%s", productId, deviceName);     // 属性上报
    sprintf(thingModelTopicPropDown, "$thing/down/property/%s/%s", productId, deviceName); // 属性下发与属性上报响应
    sprintf(thingModelTopicEventUp, "$thing/up/event/%s/%s", productId, deviceName);       // 事件上报
    sprintf(thingModelTopicEventDown, "$thing/down/event/%s/%s", productId, deviceName);   // 事件上报响应
    sprintf(thingModelTopicActionUp, "$thing/up/action/%s/%s", productId, deviceName);     // 设备响应行为执行结果
    sprintf(thingModelTopicActionDown, "$thing/down/action/%s/%s", productId, deviceName); // 应用调用设备行为

    // 1. 生成 connid 为一个随机数字，方便后台定位问题
    int connid = random(10000, 99999);
    // 2. 生成 MQTT 的 clientid 部分, 格式为 ${productid}${devicename}
    sprintf(mqttClientId, "%s%s", productId, deviceName);
    // 3. 生成 MQTT 的 username 部分, 格式为 ${clientid};${sdkappid};${connid};${expiry}
    sprintf(mqttUsername, "%s;12010126;%d;%ld", mqttClientId, connid, expiry);
    // 4. 对 username 进行签名，生成token
    char rawKey[base64::decodeLength(deviceSecret)];
    base64::decode(deviceSecret, (uint8_t *)rawKey);
    // 5. 根据物联网通信平台规则生成 password 字段
    String pwd = hmac256(mqttUsername, rawKey) + ";hmacsha256";
    strcpy(mqttPwd, pwd.c_str());
    
    // 连接MQTT服务器
    pubSubClient->setServer(mqttDomain, MQTT_PORT); 
    pubSubClient->setCallback(callback);
    mqttReconnect();
    // 订阅下行topic
    pubSubClient->subscribe(thingModelTopicPropDown);
    pubSubClient->subscribe(thingModelTopicActionDown);
}

void TencentCloudIoTSDK::loop()
{
    pubSubClient->loop();
    if (millis() - lastConnCheckTime >= LOOP_CHECK_INTERVAL)
    {
        lastConnCheckTime = millis();
        mqttReconnect();
    }
    messageBufferCheck();
}

void TencentCloudIoTSDK::sendEvent(const char *eventId, const char *params, EventType eventType)
{
    String eventTypeStr;
    switch (eventType)
    {
    case alert:
        eventTypeStr = "alert";
        break;
    case fault:
        eventTypeStr = "fault";
        break;
    default:
        eventTypeStr = "info";
    }
    char jsonBuf[1024];
    int clientToken = random(0, 99999);
    sprintf(jsonBuf, THING_MODEL_EVENT_BODY_FORMAT, clientToken, eventId, eventTypeStr, params);
    Serial.println(jsonBuf);
    boolean d = pubSubClient->publish(thingModelTopicEventUp, jsonBuf);
    Serial.print("publish:0 成功:");
    Serial.println(d);
}

void TencentCloudIoTSDK::sendProperties(const char *params)
{
    char jsonBuf[1024];
    int clientToken = random(0, 99999);
    sprintf(jsonBuf, THING_MODEL_PROP_BODY_FORMAT, clientToken, params);
    Serial.println(jsonBuf);
    boolean d = pubSubClient->publish(thingModelTopicPropUp, jsonBuf);
    Serial.print("publish:0 成功:");
    Serial.println(d);
}
void TencentCloudIoTSDK::sendProperty(char *propId, float number)
{
    addMessageToBuffer(propId, String(number));
}
void TencentCloudIoTSDK::sendProperty(char *propId, int number)
{
    addMessageToBuffer(propId, String(number));
}
void TencentCloudIoTSDK::sendProperty(char *propId, double number)
{
    addMessageToBuffer(propId, String(number));
}

void TencentCloudIoTSDK::sendProperty(char *propId, char *text)
{
    addMessageToBuffer(propId, "\"" + String(text) + "\"");
}

int TencentCloudIoTSDK::bindProperty(char *propId, CallbackFun callback)
{
    int i;
    for (i = 0; i < CALLBACK_MAP_SIZE; i++)
    {
        if (!callbackMap[i].callback)
        {
            callbackMap[i].id = propId;
            callbackMap[i].callback = callback;
            return 0;
        }
    }
    return -1;
}

int TencentCloudIoTSDK::bindAction(char *actionId, CallbackFun callback)
{
    int i;
    for (i = 0; i < CALLBACK_MAP_SIZE; i++)
    {
        if (!callbackMap[i].callback)
        {
            callbackMap[i].id = actionId;
            callbackMap[i].callback = callback;
            return 0;
        }
    }
    return -1;
}