// Encryption function helpers

// X25519 define
#include <Curve25519.h>
#include <SHA256.h>
#include <AES.h>
#include <CTR.h>

// Print hex to serial
void printHex(const char* label, const uint8_t* data, size_t len) {
  Serial.print(label);
  for (size_t i = 0; i < len; i++) {
    if (data[i] < 0x10) Serial.print('0');
    Serial.print(data[i], HEX);
  }
  Serial.println();
}

/* Calculate Curve25519 public and privet key */
void CalcKeys(uint8_t publicKey[32], uint8_t privateKey[32]){
  Curve25519::dh1(publicKey, privateKey);
  printHex("[*] Local Private Key: ", privateKey, 32);
  printHex("[*] Local Public Key: ", publicKey, 32);
}

/*Calculate shared secret as hash sha256 
input Shared secret
Output unit8_t[32] Shared Secret SAH256 Hash */ 
void CalculHashSecret(uint8_t* SharedSecret, uint8_t* HashSecret) {
  SHA256 sha256;
  sha256.reset();
  sha256.update(SharedSecret, 32);
  sha256.finalize(HashSecret, 32);
}

// Compute X25519 shared secret from remote public key and local private key
bool computeSharedSecret(uint8_t* sharedSecretOut, const uint8_t* peerPublicKey, const uint8_t* myPrivateKey) {
  memcpy(sharedSecretOut, peerPublicKey, 32);
  uint8_t privateKeyMutable[32];
  memcpy(privateKeyMutable, myPrivateKey, 32);
  return Curve25519::dh2(sharedSecretOut, privateKeyMutable);
}

/* Remote public key received process
input Remote public key */
void RemoteKeyReceivedProcess(const uint8_t *data){
  // compute shared secret
    printHex("[V] Received public key: ", data, 32);
  computeSharedSecret(sharedA, data, privateKey);
    printHex("[*] Shared secret: ", sharedA, 32);
  CalculHashSecret(sharedA, HashSecret); //Calculate secret HASH
    printHex("[*] Calculated Secret HASH: ", HashSecret, 32);
  // Bind both MACs to the fresh HashSecret as 32-byte salted digests.
  hashMACSalted(LocalMacInt_global, HashSecret, 32, LocalMacHashed);
  hashMACSalted(RemotemacInt,       HashSecret, 32, RemoteMacHashed);
    printHex("[*] Local  MAC hashed: ", LocalMacHashed,  32);
    printHex("[*] Remote MAC hashed: ", RemoteMacHashed, 32);
  // Persist only the salted remote-MAC digest (no plaintext MAC at rest).
  preferences.putBytes(RemoteMACHash.c_str(), RemoteMacHashed, 32);
  RemotePaired = true;
// do not Save hashed secret in preferances as you send public key via LoRa every new session
}

// Generate a buffer of random bytes of given length
uint8_t* generateRandomIV(size_t len) {
  uint8_t* buf = (uint8_t*)malloc(len);
  if (!buf) return nullptr;
  for (size_t i = 0; i < len; ++i) {
    buf[i] = esp_random();
  }
  return buf;
}

// Encrypt a payload with AES-256-CTR
size_t encryptPayload(const String& payload, const uint8_t* key, const uint8_t* iv, uint8_t* ciphertext) {
  CTR<AES256> aes;
  aes.clear(); // Clear any previous state
  aes.setKey(key, 32);
  aes.setIV(iv, 16);
  size_t len = payload.length();
  uint8_t plaintext[len];
  memcpy(plaintext, payload.c_str(), len);
  aes.encrypt(ciphertext, plaintext, len);
  return len;
}

// Decrypt a payload with AES-256-CTR
void decryptPayload(const uint8_t* ciphertext, size_t len, const uint8_t* key, const uint8_t* iv, uint8_t* decrypted) {
  CTR<AES256> aes;
  aes.clear(); // Clear any previous state
  aes.setKey(key, 32);
  aes.setIV(iv, 16);
  aes.decrypt(decrypted, ciphertext, len);
}

/*  Calcutate HMAC
    input: encrypted payload, encrypted payload length, HashSecret
    Output: 32 HEX HMAC */ 
void calculateHMAC(const uint8_t* data, size_t len, const uint8_t* key, uint8_t* hmac) {
    SHA256 sha256;
    sha256.resetHMAC(key, 32);
    sha256.update(data, len);
    sha256.finalizeHMAC(key, 32, hmac, 32);
}