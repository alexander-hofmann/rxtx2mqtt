/************************************************************
 rxtx2mqtt code                   
 V0.2.0                          
 (c) 2016 hofmann engineering    
                                 
 So this code will run on an Adafruit 
 HUZZAH ESP8266 Board. To program the board do:
   (1) Hold down the GPIO0 button, the red LED will be lit
   (2) While holding down GPIO0, click the RESET button
   (3) Release RESET, then release GPIO0
   (4) When you release the RESET button, the red LED will 
      be lit dimly, this means its ready to bootload
 Upload Speed is 115200 baud.
*************************************************************/  
#include <ESP8266WiFi.h>
#include <Wire.h>
#include <PubSubClient.h>

//#define DEBUG 1                           //Use this in debugging environment only  
#define SERIAL_BAUD_RATE  115200
#define WIFI_SSID "HWLAN"                   //WIFI SSID of your NETWORK
#define WIFI_PASSWORD "1234567890123456"    //WIFI Password of your NETWORK

#define MQTT_SERVER_IP "192.168.178.2"         //MQTT Server Address within your NETWORT
#define MQTT_PORT 1883                         //MQTT standard port is 1883
#define MQTT_CLIENT_NAME  "rxtx2mqtt01"        //MQTT client name - here default is rxtx2mqtt01   
#define READY_MESSAGE "READY\n"                //Welcome message when module is read - can be parsed at other side of the rxtx connection

WiFiClient espClient;                       //WiFiClient Object, needed to connect
PubSubClient client(espClient);             //Connection to MQTT Server

void setup_wifi();    //forward declaration to function setup_wifi();
void resetbuffers();  //forward declaration to function resetbuffers(); 

char inbuffer[256];   //inbuffer - all incoming bytes are stored here until the parser finds char(13) or '=' or '@' as endings
char key[256];        //the key goes into here, will be used for the topic then in the mqtt message
char value[256];      //the value goes into here, will be used for the message then in the mqtt message
int inptr = 0;        //inptr points at current position within inbuffer
char inbyte = 0;      //incoming byte that will be handled

/************************************
 function setup                  
   initializes everything        
 parameter none                  
 return none                     
************************************/
void setup() {
  Serial.begin(SERIAL_BAUD_RATE);   //initialize the Serial object with speed of SERIAL_BAUD_RATE
  setup_wifi();                     //initialize the wifi connection
  client.setServer(MQTT_SERVER_IP, MQTT_PORT);  //initialize the connection to the mqtt_server
  resetbuffers();                   //reset all buffers
  Serial.print(READY_MESSAGE);
}

/************************************
 function update                  
   will be called on receive of a new
   mqtt message.        
 parameter 
   char* topic contains the topic
   byte* payload contains the message
   unsigned int length of the message                  
 return none                     
************************************/
void update(char* topic, byte* payload, unsigned int length) {
  int i=0;
  char message_buff[255];
  for(i=0; i<length; i++) {
    message_buff[i] = payload[i];
  }
  message_buff[i] = '\0';
  Serial.print(topic);
  Serial.print("=");
  Serial.print(message_buff);
  Serial.print("\n");
}

/************************************
 function resetbuffers                  
   resets the reading buffers for in-
   coming bytes over serial        
 parameter none                  
 return none                     
************************************/
void resetbuffers() {
  memset(inbuffer, '\0', sizeof(inbuffer));   //initialize inbuffer with \0 for easier string handling
  memset(key, '\0', sizeof(key));             //initialize key with \0
  memset(value, '\0', sizeof(value));         //initialize value with \0
  inptr = 0;                                  //set inptr to the beginning, so 0
  inbyte = '\0';                              //not really needed, but who knows :-)
}
/************************************
 function setup_wifi                  
   starts the WiFi object and connects        
 parameter none                  
 return none                     
************************************/
void setup_wifi() {
  delay(10);
  #ifdef DEBUG
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(WIFI_SSID);
  #endif
  
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);     //connect to WiFi SSID and PASSWORD

  while (WiFi.status() != WL_CONNECTED) {   //wait until connected
    delay(500);
    #ifdef DEBUG
      Serial.print(".");
    #endif
  }
  #ifdef DEBUG
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
  #endif
}
/************************************
 function reconnect                  
   restarts the MQTT connection        
 parameter none                  
 return none                     
************************************/
void reconnect() {
  while (!client.connected()) {                         //try until connected
    #ifdef DEBUG
      Serial.println("Attempting MQTT connection...");
    #endif
    if (client.connect(MQTT_CLIENT_NAME)) {                  //client should connect
      #ifdef DEBUG
        Serial.println("connected");
      #endif
      client.setCallback(update);
    } else {
      #ifdef DEBUG
        Serial.print("failed, rc=");
        Serial.print(client.state());
        Serial.println(" try again in 5 seconds");
      #endif
      // Wait 2 seconds before retrying
      delay(2000);
    }
  }
}
/************************************
 function loop                  
   loop function does
     (1) reconnect if connection lost
     (2) single parse incoming message
     (3) when message is coomplete post mqtt message        
 parameter none                  
 return none                     
************************************/
void loop() {
  if (!client.connected()) {    //if connection is lost, reconnect
    reconnect();
  }
  client.loop();
  //single parse - parses message at once, splits the message into key and value pairs
  //and publishes a mqtt message if message end '@' or char(13) is detected whereas
  //key is the topic and value is the message.
  if (Serial.available() > 0) {       //if bytes are available at serial connection
    inbyte = Serial.read();           //read one byte after another into inbyte variable
    #ifdef DEBUG
      Serial.print(inbyte);
      Serial.print(" ");
    #endif
    //on buffer overflow - omit message
    if (inptr >= 254) {               //message size exceeded
      resetbuffers();                 //if message needs to be omitted - reset all buffers
      #ifdef DEBUG
        Serial.println("buffer overflow, message oversize - omitting message");
      #endif  
    } else {
      if ('?' == inbyte) {            //if ? is detected, key before is the topic to subscribe
          #ifdef DEBUG 
            Serial.println("");
            Serial.println("? sign detected");
          #endif
          strcpy(key, inbuffer);        //copy the key into the key variable
          boolean rc = client.subscribe(key);   //subscribe the topic to follow
          #ifdef DEBUG
            if (rc) {
              Serial.println("subscripted topic");      
            }
          #endif
          resetbuffers();     //reset buffers to receive the next bytes
      } else {
        if ('=' == inbyte) {            //'=' char received - extract the key value now
          #ifdef DEBUG 
            Serial.println("");
            Serial.println("= sign detected");
          #endif
          strcpy(key, inbuffer);        //copy the key into the key variable
          memset(inbuffer, '\0', sizeof(inbuffer));   //and reset the inbuffer
          inptr = 0;                    //set ptr to 0  
          #ifdef DEBUG
            Serial.print("key=");
            Serial.println(key);
          #endif
        } else {
          if ('\n' == inbyte) {    //end of message detected
            if (('\0' == key[0]) || ('\0' == inbyte)) {
              #ifdef DEBUG
                Serial.println("");
                Serial.println("end byte detected, but key or value is empty.");
              #endif      
            } else {
              #ifdef DEBUG
                Serial.println("");
                Serial.println("end byte detected");
              #endif
              strcpy(value, inbuffer);         //extract the value
              #ifdef DEBUG
                Serial.print("value=");
                Serial.println(value);
              #endif
              client.publish(key, value);     //publish topic=key message=value
              resetbuffers();                 //reset all buffers
            }
          } else {
            inbuffer[inptr++] = inbyte;     //else store next incoming byte and increase the pointer afterwards
          }
        }
      }
    }
  }
}
