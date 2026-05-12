//  This is general function helper

  //initialize preferences
void intPref(){
  preferences.begin("3NCRYPT2P", false); // The second parameter is false for read/write access. Use true for read-only.
  if (AIM){
    preferences.clear();
  }
  // Legacy migration: drop plaintext "RemoteMAC" if present and no encrypted blob yet.
  if (preferences.isKey(RemoteMAC.c_str()) && !preferences.isKey(RemoteMACHash.c_str())) {
    preferences.remove(RemoteMAC.c_str());
    Serial.println("[*] Removed legacy plaintext RemoteMAC from NVS");
  }
  Serial.println("[V] preferences initize complete");
}

  // This is a lock loop, it will lock unit for to many errors
void LockLoop(){
  preferences.putBool(DevLock.c_str(),true);
  preferences.end();
  Serial.println ("To many errors - unit will lock !\nResync keys to enable unit transmission");
    setLEDColor(COLOR_RED, 75); // 75% brightness
  while (true){
    Serial.print(" .");
    delay(1000);
  }
}

// check if HashSecret is empty
bool isHashSecretZero(const uint8_t *hash, size_t len) {
  for (size_t i = 0; i < len; i++) {
    if (hash[i] != 0) return false;
  }
  return true;
}

// Get local machine WiFi MAC address as integer
uint64_t GetLocalMAC_int(){
  uint8_t localMac[6];
  esp_read_mac(localMac, ESP_MAC_WIFI_STA);  // Get the MAC address from the Wi-Fi STA interface 
  uint64_t localMacInt = 0; // MAC address of local device
  for (int i = 0; i < 6; i++) {
    localMacInt = (localMacInt << 8) | localMac[i];
  }
  Serial.print("[*] Device MAC Address as integer: ");
  Serial.println(localMacInt);
  return localMacInt;
}

/* Salted MAC digest: out = SHA-256( macBytes || salt ).
   Salt is HashSecret so the digest is bound to the current pairing session;
   a flash dump alone reveals neither the raw MAC nor a usable HMAC input. */
void hashMACSalted(uint64_t mac, const uint8_t* salt, size_t saltLen, uint8_t out[32]) {
  SHA256 sha256;
  sha256.reset();
  sha256.update(&mac, sizeof(mac));
  if (salt && saltLen) sha256.update(salt, saltLen);
  sha256.finalize(out, 32);
}

/* prepare HMAC calculation
input: Cipher text, Cipher text length, 32-byte hashed MAC (local or remote), random IV
Output: 32 HEX HMAC
*/
uint8_t* HMACHelper(const uint8_t* ciphertext, size_t cipherLen, const uint8_t* macHash32, const uint8_t* RandIV){
  size_t inputLen = cipherLen + 16 + 32;
  uint8_t* hmacInput = (uint8_t*)malloc(inputLen);
  if (!hmacInput) return nullptr;
  memcpy(hmacInput, ciphertext, cipherLen);
  memcpy(hmacInput + cipherLen, RandIV, 16);
  memcpy(hmacInput + cipherLen + 16, macHash32, 32);
  uint8_t* hmacOut = (uint8_t*)malloc(32);
  if (!hmacOut) {
    free(hmacInput);
    return nullptr;
  }
  calculateHMAC(hmacInput, inputLen, HashSecret, hmacOut);
  free(hmacInput);
  return hmacOut;
}