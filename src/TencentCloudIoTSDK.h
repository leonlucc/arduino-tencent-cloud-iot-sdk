#ifndef TencentCloud_IOT_SDK_H
#define TencentCloud_IOT_SDK_H
#include <Arduino.h>
#include <ArduinoJson.h>
#include "Client.h"
#include <PubSubClient.h>

#define DEFAULT_EXPIRY 1924963199L // 2030-12-31 23:59:59
namespace TencentCloudIoTSDK
{
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

}
#endif
