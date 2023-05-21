#include <WiFi.h>
#include <PubSubClient.h>

// WiFi
const char *ssid = "XXX";       // Enter your WiFi name
const char *password = "XXX"; // Enter WiFi password

// NOTE: you can use https://github.com/leonlucc/tencent-cloud-iot-mqtt-tool to generate the mqtt parameters.

// MQTT Broker
const char *mqtt_broker = "ProductId.iotcloud.tencentdevices.com";
const int mqtt_port = 1883;

// MQTT Client
const char *client_id = "ProductIdDeviceName";
const char *mqtt_username = "ProductIdDeviceName;12010126;G8R3D;1682525496";
const char *mqtt_password = "a1a7568c87502f32db14766972beaacca480c7aded48d36307ce77e06535af83;hmacsha256";

// MQTT Topic
const char *up_topic = "$thing/up/property/ProductId/DeviceName";
const char *down_topic = "$thing/down/property/ProductId/DeviceName";
const char *up_msg = "{\"method\":\"report\",\"clientToken\":\"12345\",\"params\":{\"color\":1,\"power_switch\":1}}";

WiFiClient espClient;
PubSubClient client(espClient);

char up_message_buffer[256];

void setup()
{
    // Set software serial baud to 115200;
    Serial.begin(115200);
    // connecting to a WiFi network
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.println("Connecting to WiFi..");
    }
    Serial.println("Connected to the WiFi network");
    // connecting to a mqtt broker
    client.setServer(mqtt_broker, mqtt_port);
    client.setCallback(callback);
    while (!client.connected())
    {
        if (client.connect(client_id, mqtt_username, mqtt_password))
        {
            Serial.println("Tencent IOT mqtt broker connected");
        }
        else
        {
            Serial.print("failed with state ");
            Serial.println(client.state());
            delay(2000);
        }
    }
    client.subscribe(down_topic);
    client.publish(up_topic, up_msg);
}

void callback(char *topic, byte *payload, unsigned int length)
{
    Serial.print("Message arrived in topic: ");
    Serial.println(topic);
    Serial.print("Message:");
    for (int i = 0; i < length; i++)
    {
        Serial.print((char)payload[i]);
    }
    Serial.println();
    Serial.println("-----------------------");
}

void loop()
{
    client.loop();
}
