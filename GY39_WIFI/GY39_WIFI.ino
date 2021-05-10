#include <ESP8266WiFi.h>
/* 依赖 PubSubClient 2.4.0 */
#include <PubSubClient.h>
/* 依赖 ArduinoJson 5.13.4 */
#include <ArduinoJson.h>
#include <Wire.h>

/* 连接您的WIFI SSID和密码 */
#define WIFI_SSID         "你的wifi"
#define WIFI_PASSWD       "你的密码"


/* 设备的三元组信息*/
#define PRODUCT_KEY       "你的key"       
#define DEVICE_NAME       "你的设备名"          
#define DEVICE_SECRET     "你的密钥"    
#define REGION_ID         "cn-shanghai"

/* 线上环境域名和端口号，不需要改 */
#define MQTT_SERVER       PRODUCT_KEY ".iot-as-mqtt." REGION_ID ".aliyuncs.com"
#define MQTT_PORT         1883
#define MQTT_USRNAME      DEVICE_NAME "&" PRODUCT_KEY

#define CLIENT_ID         "ESP8266|securemode=3,timestamp=1620094295,signmethod=hmacsha1|"
// 算法工具: http://iot-face.oss-cn-shanghai.aliyuncs.com/tools.htm 进行加密生成password
// password教程 https://www.yuque.com/cloud-dev/iot-tech/mebm5g
#define MQTT_PASSWD       "2b6a65c54afbc71c859dd4d1dcacs18f88fd6a2a"  //"参考上面password教程，算法工具生成"

#define ALINK_BODY_FORMAT         "{\"id\":\"123\",\"version\":\"1.0\",\"method\":\"thing.event.property.post\",\"params\":%s}"
#define ALINK_TOPIC_PROP_POST     "/sys/" PRODUCT_KEY "/" DEVICE_NAME "/thing/event/property/post"

#define uint16_t unsigned int
#define iic_add  0x5b
//D1 SCL,D2 SDA
unsigned long lastMs = 0;
WiFiClient espClient;
PubSubClient  client(espClient);

float LUX_sum,P_sum, RH_sum,T_sum;

unsigned char count=0;

typedef struct
{
    uint32_t P;
    uint16_t Temp;
    uint16_t Hum;
    uint16_t Alt;
} bme;

bme Bme;
uint32_t Lux;

void i2c_speed(uint16_t scl_speed)
{
  /* initialize TWI clock: 40 kHz clock, TWPS = 0 => prescaler = 1 */
//  TWSR = 0;                         /* no prescaler */
//  TWBR = ((F_CPU/scl_speed)-16)/2;  /* must be > 10 for stable operation */
  Wire.setClock(scl_speed);
}

void get_bme(void)
{
   uint16_t data_16[2]={0};
   uint8_t data[10]={0};
   iic_read(0x04,data,10);
    //iic_read2(0x04,data,10);
   Bme.Temp=(data[0]<<8)|data[1];
   data_16[0]=(data[2]<<8)|data[3];
   data_16[1]=(data[4]<<8)|data[5];
   Bme.P=(((uint32_t)data_16[0])<<16)|data_16[1];
   Bme.Hum=(data[6]<<8)|data[7];
   Bme.Alt=(data[8]<<8)|data[9];
}
void get_lux(void)
{     
    uint16_t data_16[2]={0};
    uint8_t data[10]={0};
    //iic_read2(0x00,data,4);
    iic_read(0x00,data,4);
    data_16[0]=(data[0]<<8)|data[1];
    data_16[1]=(data[2]<<8)|data[3];
    Lux=(((uint32_t)data_16[0])<<16)|data_16[1];

}
void iic_read(unsigned char reg,unsigned char *data,uint8_t len )
{
   Wire.beginTransmission(iic_add);  
   Wire.write(reg); 
   Wire.endTransmission(false);
   delayMicroseconds(10);
   if(len>4){
     Wire.requestFrom(iic_add,10); 
   }else{
     Wire.requestFrom(iic_add,4);
   }
   for (uint8_t i = 0; i < len; i++)
   {
    data[i] = Wire.read(); 
   }
}



void callback(char *topic, byte *payload, unsigned int length)
{
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");
    payload[length] = '\0';
    Serial.println((char *)payload);

}


void wifiInit()
{
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWD);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(1000);
        Serial.println("WiFi not Connect");
    }
    Serial.println("Connected to AP");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());  
    Serial.print("espClient [");
    client.setServer(MQTT_SERVER, MQTT_PORT);   /* 连接WiFi之后，连接MQTT服务器 */
    client.setCallback(callback);
}


void mqttCheckConnect()
{
    while (!client.connected())
    {
        Serial.println("Connecting to MQTT Server ...");
        if (client.connect(CLIENT_ID, MQTT_USRNAME, MQTT_PASSWD))
        {
            Serial.println("MQTT Connected!");
        }
        else
        {
            Serial.print("MQTT Connect err:");
            Serial.println(client.state());
            delay(5000);
        }
    }
}


void mqttIntervalPost()
{
    char param[32];
    char jsonBuf[128];
    //=========================Temperature===============================
    sprintf(param, "{\"CurrentTemperature\":%f}",T_sum/count);
    sprintf(jsonBuf, ALINK_BODY_FORMAT, param);
    Serial.println(jsonBuf);
    boolean d = client.publish(ALINK_TOPIC_PROP_POST, jsonBuf);
    if(d){
      Serial.println("publish Temperature success"); 
    }else{
      Serial.println("publish Temperature fail"); 
    }
    //==============================Humidity===============================
    sprintf(param, "{\"CurrentHumidity\":%f}",RH_sum/count);
    sprintf(jsonBuf, ALINK_BODY_FORMAT, param);
    Serial.println(jsonBuf);
    d = client.publish(ALINK_TOPIC_PROP_POST, jsonBuf);
    if(d){
      Serial.println("publish Humidity success"); 
    }else{
      Serial.println("publish Humidity fail"); 
    }
    //==============================Pressure============================================
    sprintf(param, "{\"Airpressure\":%f}",P_sum/count);
    sprintf(jsonBuf, ALINK_BODY_FORMAT, param);
    Serial.println(jsonBuf);
    d = client.publish(ALINK_TOPIC_PROP_POST, jsonBuf);
    if(d){
      Serial.println("publish Pressure success"); 
    }else{
      Serial.println("publish Pressure fail"); 
    }
    //==========================Lux=============================
    sprintf(param, "{\"LightLux\":%f}",LUX_sum/count);
    sprintf(jsonBuf, ALINK_BODY_FORMAT, param);
    Serial.println(jsonBuf);
    d = client.publish(ALINK_TOPIC_PROP_POST, jsonBuf);
    if(d){
      Serial.println("publish Lux success"); 
    }else{
      Serial.println("publish Lux fail"); 
    }
}


void setup() 
{
    /* initialize serial for debugging */
    Serial.begin(115200);
    Serial.println("Demo Start");

    wifiInit();
    Wire.begin();
    i2c_speed(40000);
    delay(1); 

}

// the loop function runs over and over again forever
void loop()
{
//    if (digitalRead(SENSOR_PIN) == HIGH){
      delay(5*1000);
      Serial.println("Motion detected!");
      get_bme();
      Serial.print("Temp: ");
      Serial.print( (float)Bme.Temp/100);
      Serial.print(" DegC  PRESS : ");
      Serial.print( ((float)Bme.P)/100);
      Serial.print(" Pa  HUM : ");
      Serial.print( (float)Bme.Hum/100);
      Serial.print(" % ALT:");
      Serial.print( Bme.Alt);
      Serial.println("m");
      get_lux();
      Serial.print( "Lux: ");
      Serial.print( ((float)Lux)/100);
      Serial.println(" lux");
      
      RH_sum =RH_sum + (float)Bme.Hum/100;
      T_sum  =T_sum + (float)Bme.Temp/100;
      LUX_sum = LUX_sum + (float)Lux/100;
      P_sum = P_sum + (float)Bme.P/100;
      count+=1;
      if (millis() - lastMs >= 20*1000)
      {
          lastMs = millis();
          mqttCheckConnect(); 
          /* 上报 */
          mqttIntervalPost();
          count=0;
          RH_sum=0;
          T_sum=0;
          LUX_sum = 0;
          P_sum = 0;
      }
      client.loop();
 //   }else{
//      Serial.println("Motion absent!");
//      delay(2000);
//  }

}
