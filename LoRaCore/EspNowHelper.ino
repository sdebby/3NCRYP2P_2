// ESP Now function helpers

// ESP Now setup parameters
void ESPNowSetup(){
  // Create a broadcast peer object
  WiFi.mode(WIFI_STA);
  WiFi.setChannel(ESPNOW_WIFI_CHANNEL);
  while (!WiFi.STA.started()) {
    delay(100);
  }
    
  if (!Consl) {// if debug -> Display parameters
    Serial.println("[*] Wi-Fi parameters:");
    Serial.println("[*] Mode: STA");
    Serial.println("[*] MAC Address: " + WiFi.macAddress());
    Serial.printf("[*] Channel: %d\n", ESPNOW_WIFI_CHANNEL);
    Serial.printf("[*] ESP-NOW version: %d, max data length: %d\n", ESP_NOW.getVersion(), ESP_NOW.getMaxDataLen());
  }
  Serial.println("[V] ESP NOW initize complete");
}

/* Excnahge public keys via ESP NOW protocol
*/
void ExchangePublicKeysESPNOW(){
    ESPNowSetup();
  if (!broadcast_peer.begin()) {
    Serial.println("[X] Failed to initialize broadcast peer");
    Serial.println("[X] Rebooting in 3 seconds...");
    printSCR("Failed to initize ESP NOW, rebooting", 0);
    delay(3000);
    ESP.restart();
  }
ESP_NOW.onNewPeer(onNewPeer, nullptr); // Register callback for handling messages from unknown peers
// Sending public key
// TODO send also software versions and compare
bool HashMSGFlag = false;
bool SendTestMSGFlag = false;
while (KeySendCounter < KeySendTries ) {  // Send public key via ESP NOW until KeySendTries
  if (PubKeySendTimer.call() ){ 
    if (!broadcast_peer.send_message((uint8_t *)publicKey, sizeof(publicKey))) {
      Serial.println("[X] Failed to broadcast message");
      printSCR("Failed to send message",0);
      } else {
        Serial.printf("[*] Public key broadcast attempt %d\n", KeySendCounter + 1);
      }
      KeySendCounter++;
      HashMSGFlag = false;
  }
}
printSCR("pub key sent via ESPNOW",0);
  // send test message
  delay(1000);
if (!isHashSecretZero(HashSecret, sizeof(HashSecret)) && !SendTestMSGFlag ) { //Check if Hash secret is empty
  if (SendFirst){ //if local MAC address > remote MAC address
    Serial.println("[*] Sending test payload via LoRa ...");
    printSCR("Sending test payload",0);
    SendLoRaPayload(1, TestPayload);
    SendTestMSGFlag = true; //Send message only once
  } 
  } else {
    if (!HashMSGFlag){
    Serial.println("[X] HashSecret is empty cannot send test message");
    HashMSGFlag = true;
    }
    // TODO handle fail hash secret
  }
}