/* 27.02.2026
3NCRYPT2P project
__ Omni Screen and keyboard program __
__ Enhance security ___

*/
#define SoftwareVer 260501
#include <SPI.h>
#include <LoRa.h>
#include <Preferences.h>
#include "esp32-hal-log.h"
#include <Adafruit_NeoPixel.h> //for internal LED
  constexpr uint8_t LED_PIN = 8;
  constexpr uint8_t NUM_LEDS = 1;
#include "ptScheduler.h"

// Define screen and keyboard
#include <Wire.h>
#include <Adafruit_SSD1306.h>

#define CARDKB_ADDR 0x5F 
#define SCRN_ADDR 0x3C 

// Define screen width and height
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define MAX_LINES 6  // Maximum number of lines that can fit on screen
#define LINE_HEIGHT 8  // Height of each line in pixels
#define LINE_BUFFER 20 // Total history lines preserved

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
int TextSize = 1;
String lines[LINE_BUFFER];  // Array to store history lines
int totalLines = 0;          // How many lines written so far
int scrollOffset = 0;        // Scroll position (0 = newest visible)
int currentLine = 0;         // Current line position

#if CONFIG_IDF_TARGET_ESP32C6  // Define LoRa-ESP32 pin out for ESP32 - C6
  #define BoardMSG  "[*] ESP32-C6 board found"
  #include <WiFi.h>
  #define ss 18
  #define rst 23
  #define dio0 22
  #define WireSDA 3
  #define WireSCL 2

#elif CONFIG_IDF_TARGET_ESP32S3
  #define BoardMSG "[*] ESP32-S3 board found"

#else
  #error "[X] The code will run on ESP32 C6 only."
#endif

#define MAX_PAYLOAD_SIZE 255
#define KEY_ENTER 13
#define KEY_BACKSPACE 8
#define KEY_UP 181
#define KEY_DOWN 182
#define KEYBOARD_KEY_EXCHANGE 161 //function + k
#define KEYBOARD_KEY_AIM 156 //function + d (DEBUG)

bool Consl = false; // Console/debug mode
bool AIM = false; // AIM = Automated Interval Messages

bool SendFirst = false; // send sequence = true if LocalMacInt_global > RemotemacInt)

  // Define Encryption key
  uint8_t sharedA[32]; //Shared secret
  uint8_t publicKey[32];
  uint8_t privateKey[32];
  uint8_t HashSecret[32];
  // Debug - send messages in intervals
  int counter = 0;
int ReceiveErrorCount = 0;  // Count of failed HMAC verification on received messages
const int MAX_RECEIVE_ERRORS = 10;

String ReceivedMSG_Pack = "";
const int RTN_OK = 200;
const int RTN_NG = 502;
const int RTN_RESYNC = 500;
bool ackReceived = false;       // Flag set by ACK handler (case 3)
const int SEND_RETRIES = 3;     // Max send attempts
const int ACK_TIMEOUT_MS = 2000; // Wait time for ACK per attempt

Preferences preferences;
String PayloadInput = "";
String ReSendPayloadInput = "";


// Saved parameters under 3NRYP2P project
String DevLock = "DeviceLOCK";
String RemoteMAC = "RemoteMAC";          // legacy plaintext key, removed on boot if present
String RemoteMACHash = "RemoteMACH";     // 32-byte salted SHA-256 of remote MAC
uint64_t RemotemacInt = 0;               // RAM only; populated during pairing, used for send-order compare
uint64_t LocalMacInt_global = 0;
uint8_t  RemoteMacHashed[32] = {0};      // SHA-256(remoteMac || HashSecret)
uint8_t  LocalMacHashed[32]  = {0};      // SHA-256(localMac  || HashSecret)
bool     RemotePaired = false;           // true once RemoteMACHash blob is loaded/written
String TestPayload = "CONFIRM";
bool TestPayloadSent = false; //test payload sent flag
int KeySendCounter = 0;
int KeySendTries = 3;
bool RemoteKeyReceived = false; // Remote public key received flag
ptScheduler PubKeySendTimer = ptScheduler (PT_TIME_2S); 
ptScheduler AIMInterval = ptScheduler (PT_TIME_10S);
Adafruit_NeoPixel rgbLed(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);
struct RGB {
    uint8_t r, g, b;
};
constexpr RGB COLOR_OFF   = {0, 0, 0};
constexpr RGB COLOR_RED = {255, 0, 0}; 

// ESP NOW - begin
#include "ESP32_NOW.h"
#include "WiFi.h"
#include <esp_mac.h>  // For MAC2STR and MACSTR macros

#define ESPNOW_WIFI_CHANNEL 6

/* Set RGB colors in built in led
*/
void setLEDColor(const RGB& color, uint8_t brightness = 100) {
  uint16_t scale = (uint16_t)brightness * 255 / 100;
  uint8_t r = (uint8_t)(((uint16_t)color.r * scale) / 255);
  uint8_t g = (uint8_t)(((uint16_t)color.g * scale) / 255);
  uint8_t b = (uint8_t)(((uint16_t)color.b * scale) / 255);
  rgbLed.setPixelColor(0, rgbLed.Color(r, g, b));
  rgbLed.show();
}

class ESP_NOW_Broadcast_Peer : public ESP_NOW_Peer {
public:

  ESP_NOW_Broadcast_Peer(uint8_t channel, wifi_interface_t iface, const uint8_t *lmk)
    : ESP_NOW_Peer(ESP_NOW.BROADCAST_ADDR, channel, iface, lmk) {}

  ~ESP_NOW_Broadcast_Peer() {
    remove();
  }

  bool begin() {
    if (!ESP_NOW.begin() || !add()) {
      Serial.println("[X] Failed to initialize ESP-NOW or register the broadcast peer");
      return false;
    }
    return true;
  }

  bool send_message(const uint8_t *data, size_t len) {
    if (!send(data, len)) {
      Serial.println("[X] Failed to broadcast message");
      return false;
    }
    return true;
  }

  // Override onReceive to handle incoming messages
  void onReceive(const uint8_t *data, size_t len, bool broadcast) override {
    Serial.printf("[V] (onReceive) Received a message from " MACSTR " (%s)\n", MAC2STR(addr()), broadcast ? "broadcast" : "unicast");
  }
};

ESP_NOW_Broadcast_Peer broadcast_peer(ESPNOW_WIFI_CHANNEL, WIFI_IF_STA, nullptr);

// Callback function to handle incoming messages from unknown peers
void onNewPeer(const esp_now_recv_info_t *info, const uint8_t *data, int len, void *arg) {
  RemotemacInt = 0;  // Reset before calculating
  for (int i = 0; i < 6; i++) {
    RemotemacInt = (RemotemacInt << 8) | info->src_addr[i];
  }
  Serial.printf("[V] (onNewPeer) Received message from " MACSTR " => MAC integer: %llu\n", MAC2STR(info->src_addr), RemotemacInt);
  if (!RemoteKeyReceived){ //Saving first attempt only
    RemoteKeyReceivedProcess(data); //start remote public key process , save remote mac address
    RemoteKeyReceived = true;
    if (LocalMacInt_global > RemotemacInt){
      SendFirst = true;
      Serial.println("[*] Local MAC is larger");
    } else {
      Serial.println("[*] Remote MAC is larger");
    }
  }
} 
// ESP NOW END

void setup() {
  // TODO uncomment this on production !!!
  // if (Consl){
    Serial.begin(115200);
      while (!Serial);
    Serial.printf("**-- 3NCRYP2P CORE - version %d --**\n",SoftwareVer);
  // } else {
  //   Serial.setDebugOutput(false);
  // }
    // Get compiled debug level
  #ifdef CORE_DEBUG_LEVEL
    Serial.printf("[*] CORE_DEBUG_LEVEL Value: %d\n",CORE_DEBUG_LEVEL );// Try printing its value
    #if CORE_DEBUG_LEVEL == 4// if compiled debug level = DEBUG
       Consl = true; 
    #endif
  #else
    Serial.println("[*] CORE_DEBUG_LEVEL is NOT defined.");
  #endif

  // I2C pins
  Wire.begin(WireSDA, WireSCL);
  // Initialize the display
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCRN_ADDR)) {
    Serial.println(F("[X] SSD1309 allocation failed"));
    for (;;);
  }
  intDisp();
  LogoDisplay();
  intPref();
  LocalMacInt_global = GetLocalMAC_int();
  // Check if device is locked
  if (preferences.getBool(DevLock.c_str(),false)){
    LockLoop(); // Lock device
  }
  Serial.println(BoardMSG);

  // Initialize LoRa 
  LoRa.setPins(ss, rst, dio0);
  if (!LoRa.begin(433E6)) {
    log_d("[X] Starting LoRa failed!");
    Serial.println("[X] Rebooting in 3 seconds...");
    delay(3000);
    ESP.restart();
  }
  SetLoRaParam();// LoRa parameters

  /* exchanging public keys via ESP NOW. */
  CalcKeys(publicKey,privateKey);
  if (Consl) preferences.clear(); //DEBUG - clear saved parameters
  size_t blobLen = preferences.getBytes(RemoteMACHash.c_str(), RemoteMacHashed, 32);
  RemotePaired = (blobLen == 32);
  if (!RemotePaired) { //No paired remote stored - start initial handshake
    Serial.println("[X] No paired remote found in preferences, exchanging keys process initiated");
    log_d("[*] ESP NOW - Public key exchange");
    ExchangePublicKeysESPNOW();

  } else { // Paired remote MAC digest in memory
    Serial.println("[*] Paired remote digest found in preferences");
    printSCR("SYSTEM READY !", 0);
    if (isHashSecretZero(HashSecret, sizeof(HashSecret))){ //check if has HashSecret
      printSCR("Exchanging keys", 0);
      TransmitLoRaHandshake(0, 32, publicKey,1);
      } else {
        Serial.println("[*] Hash Secret found in local device");
      }
  }
  rgbLed.begin(); //update built in LED
  rgbLed.show(); 
  setLEDColor(COLOR_OFF); 
}

void loop() {
  onReceive(LoRa.parsePacket());
   if (Consl && Serial.available()) { // Get text from serial if using debug
    PayloadInput = Serial.readStringUntil('\n');
    if (!RemotePaired){
      Serial.println("[X] No paired remote found, run exchange keys process");
      // TODO exit loop and start key exchange via ESP NOW
      delay(500);
    } else {
      if (SendWithRetry(PayloadInput)) {
        printSCR(PayloadInput, 1);
      } else {
        printSCR("No ACK received!", 0);
      }
    }
  } else { // Get text from CardKB
    if (!RemotePaired){
      Serial.println("[X] No paired remote found, run exchange keys process");
      printSCR("Missing remote MAC address, resync public keys",0);
      // TODO exit loop and start key exchange via ESP NOW
      delay(500);
      return;
    } else { //Remote MAC address != 0
      Wire.requestFrom(CARDKB_ADDR, 1);  
      if (Wire.available()) { 
        char c = Wire.read();  // Store the received data.
      if (c != 0) {
          if (c == KEY_ENTER && PayloadInput.length() > 0){  // Enter
            ReSendPayloadInput = PayloadInput; // for re sending payload if needed
            Serial.println(PayloadInput);
            if (SendWithRetry(PayloadInput)) {
              printSCR(PayloadInput, 1);
            } else {
              printSCR("No ACK received!", 0);
            }
            PayloadInput = "";
            } else if (c == KEY_BACKSPACE) {  // Backspace
          int cursorX = display.getCursorX() - 6 * TextSize;
          int cursorY = display.getCursorY();
          if (cursorX < 0) cursorX = 0 ;
          display.setCursor(cursorX, cursorY);
          display.fillRect(cursorX,cursorY,cursorX + 6 * TextSize , cursorY + LINE_HEIGHT * TextSize,BLACK);
          display.display();
          if (PayloadInput.length() > 0) {
            PayloadInput.remove (PayloadInput.length() - 1); // remove last character
          }
          c = 0;
        } else if (c == KEYBOARD_KEY_EXCHANGE){
          Serial.println("[*] ESP NOW key exchange process start");
            display.clearDisplay();
            display.setCursor(0, 0);
            display.println(F("Press ENTER when remote device is ready"));
            display.display();
          while (true) {  // Wait for KEY_ENTER from CardKB
            Wire.requestFrom(CARDKB_ADDR, 1);
            if (Wire.available()) {
              char key = Wire.read();
              if (key == KEY_ENTER) break;
            }
            delay(50);
          }
          //rastart parameters
          TestPayloadSent = false; KeySendCounter = 0; RemoteKeyReceived = false; 
          ExchangePublicKeysESPNOW();
          ESP_NOW.end();
          PayloadInput = "";
        } else if (c ==  KEYBOARD_KEY_AIM){ //change status of AIM - to send/stop auto messaging
          AIM = !AIM;
          printSCR("Start/Stop Auto msg", 0);
        }
        
         else if (c == KEY_UP) {  // Scroll up (older messages)
          scrollUp();
          c = 0;
        } else if (c == KEY_DOWN) {  // Scroll down (newer messages)
          scrollDown();
          c = 0;
        } else {
          PayloadInput += String(c);
          display.print(c);
        }
        display.display();
      }
    } else {
      Serial.println("[X] keyboard not available");
      delay(1000);
    } 
    } 
  }
  if (AIM){
    if (AIMInterval.call()){
      Serial.printf("[*] Sending DEBUG packet: %d \n", counter);
      String Ttext = "DBG PKT No.:" + String(counter);
      printSCR(Ttext,1);
      SendLoRaPayload(2, Ttext);
      counter ++;
    }
  }
  // TODO handle "sync public keys" message show in remote device - if need to sync keys then remote device need to send "resync" return message.
  delay(50);
}