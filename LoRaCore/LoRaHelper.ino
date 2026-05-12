// This is LoRa function helper

  // initialize LoRa parameters
void SetLoRaParam(){
  LoRa.setSyncWord(0xB2);  //Unique Sync Word
  LoRa.setSignalBandwidth(41.7E3); //7.8E3, 10.4E3, 15.6E3, 20.8E3, 31.25E3, 41.7E3, 62.5E3, 125E3, and 250E3.
  LoRa.setSpreadingFactor(9) ;  // Supported values are between `6` and `12`. If a spreading factor of `6` is set, implicit header mode must be used to transmit and receive packets.
  LoRa.setTxPower(20); //2-20 default = 17
  LoRa.enableCrc();  
  LoRa.setPreambleLength(14);
  LoRa.setCodingRate4(8); 
  Serial.println("[*] Sending LoRa initialize parameters.");
}

// Sending test payload to verify key exchange
void SendLoRaPayload(int setupBit , String TextPayload){
        // Generate random IV
  uint8_t* RandIV = generateRandomIV(16);
  if (RandIV) {
      // encrypt payload
    uint8_t ciphertext[TextPayload.length() + 16];  // Add buffer safety margin
    size_t cipherLen = encryptPayload(TextPayload, HashSecret, RandIV, ciphertext); // Encrypt using shared secret and IV return encrypt size
      printHex("[*] Device Encrypted payload: ", ciphertext, cipherLen);
    uint8_t* hmac = HMACHelper(ciphertext, cipherLen, LocalMacHashed, RandIV);
    printHex("[*] Local IV: ", RandIV, 16);
    printHex("[*] Calculated HMAC: ", hmac, 32);
    delay(500); // 0.5S delay
    TransmitLoRaPayload(setupBit, cipherLen, ciphertext, RandIV, hmac);
    free(hmac); 
    free(RandIV); 
  } else {
    Serial.println("[X] Failed to generate random IV");
    printSCR("Cannot generate rnd IV- restart device",0);
  }
}

// Send encrypted payload with retry - wait for ACK up to SEND_RETRIES times
bool SendWithRetry(String TextPayload){
  ackReceived = false;
  for (int attempt = 1; attempt <= SEND_RETRIES; attempt++) {
    Serial.printf("[*] Send attempt %d/%d\n", attempt, SEND_RETRIES);
    SendLoRaPayload(2, TextPayload);
    unsigned long startTime = millis();
    while (millis() - startTime < ACK_TIMEOUT_MS) {
      onReceive(LoRa.parsePacket()); // Poll for incoming ACK
      if (ackReceived) {
        // Serial.println("[V] ACK received");
        return true;
      }
      delay(50);
    }
    Serial.printf("[X] No ACK after attempt %d\n", attempt);
  }
  return false;
}

  // send payload via LoRa
void TransmitLoRaPayload(int Setup,int payloadSize, uint8_t* payload, uint8_t* iv, uint8_t* hmac){
  LoRa.beginPacket();
    LoRa.write(Setup);
    LoRa.write(payloadSize);
    LoRa.write(payload,payloadSize);
    LoRa.write(iv,16);
    LoRa.write(hmac,32);
  LoRa.endPacket();
}
// Send ACK - setup bit 3 | rtn[3] | IV[16] | HMAC[32]
// HMAC key = HashSecret, HMAC input = rtn + length(3) + LocalMAC + IV
void TransmitLoRaACK(int rtn){
  uint8_t* RandIV = generateRandomIV(16);
  if (!RandIV) {
    Serial.println("[X] Failed to generate IV for ACK");
    return;
  }
  String rtnStr = String(rtn);
  uint8_t rtnBytes[3];
  memcpy(rtnBytes, rtnStr.c_str(), 3);
  uint8_t* hmac = HMACHelper(rtnBytes, 3, LocalMacHashed, RandIV);
  if (!hmac) {
    Serial.println("[X] Failed to generate HMAC for ACK");
    free(RandIV);
    return;
  }
  LoRa.beginPacket();
    LoRa.write(3);             // setup bit 3 = ACK
    LoRa.write(rtnBytes, 3);  // RTN code as 3-byte string
    LoRa.write(RandIV, 16);   // IV
    LoRa.write(hmac, 32);     // HMAC
  LoRa.endPacket();
  Serial.printf("[*] ACK sent: %d\n", rtn);
  free(hmac);
  free(RandIV);
}

  // send handshake via LoRa
  // Wire format: [Setup 1B][pubkey size 1B][pubkey N B][senderMAC 8B][KeyReturn 1B]
  // Sender MAC is included so the receiver can recompute the salted MAC digest after a
  // cold reboot, when raw RemotemacInt is no longer in RAM (NVS now holds only the digest).
void TransmitLoRaHandshake(int Setup,int payloadSize, uint8_t* payload, int KeyReturn){
  uint8_t macBytes[8];
  for (int i = 0; i < 8; i++) macBytes[i] = (LocalMacInt_global >> (56 - i*8)) & 0xFF;
  LoRa.beginPacket();
    LoRa.write(Setup);
    LoRa.write(payloadSize);
    LoRa.write(payload,payloadSize);
    LoRa.write(macBytes, 8);
    LoRa.write(KeyReturn);
  LoRa.endPacket();
}

  // receive LoRa payload
void onReceive(int packetSize){
  if (!packetSize) return; //Drop packet
  if (packetSize > MAX_PAYLOAD_SIZE || packetSize <= 0) { //drop package if package size is larger then 255
    Serial.println("[X] Invalid payload size");
    return;
  }
    float RSSI_pre = (1.0 - (-1*LoRa.packetRssi())/125.0) * 100.0;
    Serial.printf("[*] LoRa Packet size: %d  ,SNR: %f  , signal: %f %%\n", packetSize, LoRa.packetSnr(), RSSI_pre); //print to serial
    // Reading package
  int SetupParam = LoRa.read(); // setup bit
  uint8_t PrefRemoteMacHashed[32] = {0};
  preferences.getBytes(RemoteMACHash.c_str(), PrefRemoteMacHashed, 32); // get salted remote-MAC digest
  switch (SetupParam) {
    case 0:{//LoRa handshake
      Serial.println("[*] LoRa handshake received");
      int RecKeySize = LoRa.read();
      uint8_t RecPubKey[RecKeySize];
      for (int i = 0; i < RecKeySize; i++) {
        RecPubKey[i] = LoRa.read();  //read remote public key
      }
      // Read sender MAC so we can salt-hash it with the fresh HashSecret.
      RemotemacInt = 0;
      for (int i = 0; i < 8; i++) {
        RemotemacInt = (RemotemacInt << 8) | (uint8_t)LoRa.read();
      }
      Serial.printf("[V] Handshake sender MAC int: %llu\n", RemotemacInt);
      RemoteKeyReceivedProcess(RecPubKey);
      Serial.println("[*] onReceive remote public key received");
      int KeySend = LoRa.read();
      if ((bool)KeySend){ // if send key (if remote hash secret = 0) = 1 then resend public key
        Serial.println("[*] onReceive ,KeySend = 1, sending local public key");
        TransmitLoRaHandshake(0, 32, publicKey,0);
      }
      TransmitLoRaACK(RTN_RESYNC); // Notify sender that key sync was received
      break;
    }

    case 1:{//Test message handshake
      if (isHashSecretZero(HashSecret, sizeof(HashSecret))) {
        Serial.println("[X] Hash secret is 0 - resync public keys");
        printSCR("Resync pub keys",0);
        return;
      }
      Serial.println("[*] LoRa Test message handshake received");
      int RecPayloadSize = LoRa.read();
      uint8_t RecPayload[RecPayloadSize];
      for (int i = 0; i < RecPayloadSize; i++) {
        RecPayload[i] = LoRa.read();  //read payload
      }
      uint8_t RecIV[16];
      for (int i = 0; i < 16; i++) { 
        RecIV[i] = LoRa.read();  //read iv
      }
      uint8_t RecHMAC[32];
      for (int i = 0; i < 32; i++) { 
        RecHMAC[i] = LoRa.read();  //read iv
      }
      printHex("[*] Received Encrypted payload: ", RecPayload, RecPayloadSize);
      printHex("[*] Received IV: ", RecIV, 16);
      printHex("[*] Received HMAC: ", RecHMAC, 32);
        // Decrypting message
      uint8_t decrypted[RecPayloadSize + 1];
      decryptPayload(RecPayload, RecPayloadSize, HashSecret, RecIV, decrypted);
      decrypted[RecPayloadSize] = '\0';
      String decryptedSTR = String((char*)decrypted);
      if (decryptedSTR == TestPayload) {
        Serial.println("[V] Test payload decrypted OK");
      } else {
        Serial.println("[X] Test payload fail !");
      }
        // Validating HMAC
      uint8_t* RecHmacCalc = HMACHelper(RecPayload, RecPayloadSize, PrefRemoteMacHashed, RecIV);
      printHex("[*] Received Calculated HMAC: ", RecHmacCalc, 32);
      if (memcmp(RecHmacCalc, RecHMAC, 32) == 0) {
        Serial.println("[V] HMAC is valid");
        printSCR("Key exchange process success", 0);
      } else {
        Serial.println("[X] HMAC is NOT valid");
        printSCR("Key exchange process fails!", 0);
      }
      free(RecHmacCalc);
      if (!SendFirst) { // send message
        Serial.println("[*] Sending test payload via LoRa ...");
        printSCR("LoRa-Sending text msg", 0);
        SendLoRaPayload(1, TestPayload);
      }
      break;
    }

    case 2:{//Encrypted payload message
      if (isHashSecretZero(HashSecret, sizeof(HashSecret))) {
        Serial.println("[X] Hash secret is 0- resync public keys");
        printSCR("Resync pub keys",0);
        return;
      }
      Serial.println("[*] LoRa Encrypted payload message received");
      int RecPayloadSize = LoRa.read();
      uint8_t RecPayload[RecPayloadSize];
      for (int i = 0; i < RecPayloadSize; i++) {
        RecPayload[i] = LoRa.read();  //read payload
      }
      uint8_t RecIV[16];
      for (int i = 0; i < 16; i++) { 
        RecIV[i] = LoRa.read();  //read iv
      }
      uint8_t RecHMAC[32];
      for (int i = 0; i < 32; i++) { 
        RecHMAC[i] = LoRa.read();  //read iv
      }
      printHex("[*] Received Encrypted payload: ", RecPayload, RecPayloadSize);
      printHex("[*] Received IV: ", RecIV, 16);
      printHex("[*] Received HMAC: ", RecHMAC, 32);
        // Decrypting message
      uint8_t decrypted[RecPayloadSize + 1];
      decryptPayload(RecPayload, RecPayloadSize, HashSecret, RecIV, decrypted);
      // validate received message HMAC
      uint8_t* RecHmacCalc = HMACHelper(RecPayload, RecPayloadSize, PrefRemoteMacHashed, RecIV);
      printHex("[*] Received Calculated HMAC: ", RecHmacCalc, 32);
      if (memcmp(RecHmacCalc, RecHMAC, 32) == 0) {
        ReceiveErrorCount = 0;  // Reset error count on success
        decrypted[RecPayloadSize] = '\0';
        String decryptedSTR = String((char*)decrypted);
        Serial.printf("[V] HMAC is valid, Decrypted payload: %s\n", decryptedSTR.c_str());
        printSCR(decryptedSTR, 2);
        TransmitLoRaACK(RTN_OK);  // Send ACK back to sender
      } else {
        ReceiveErrorCount++;
        Serial.printf("[X] HMAC is NOT valid (%d/%d)\n", ReceiveErrorCount, MAX_RECEIVE_ERRORS);
        printSCR("HMAC invalid! " + String(ReceiveErrorCount) + "/" + String(MAX_RECEIVE_ERRORS), 0);
        TransmitLoRaACK(RTN_NG);  // Send NACK back to sender
        if (ReceiveErrorCount >= MAX_RECEIVE_ERRORS) {
          Serial.println("[X] Max receive errors reached - locking device");
          LockLoop();
        }
      }
      free(RecHmacCalc);
      break;
    }

    case 3:{//ACK response received
      uint8_t RecRTN[3];
      for (int i = 0; i < 3; i++) {
        RecRTN[i] = LoRa.read();  // read RTN code
      }
      uint8_t RecIV[16];
      for (int i = 0; i < 16; i++) {
        RecIV[i] = LoRa.read();   // read IV
      }
      uint8_t RecHMAC[32];
      for (int i = 0; i < 32; i++) {
        RecHMAC[i] = LoRa.read(); // read HMAC
      }
      // Verify HMAC using remote MAC digest as sender
      uint8_t* RecHmacCalc = HMACHelper(RecRTN, 3, PrefRemoteMacHashed, RecIV);
      if (memcmp(RecHmacCalc, RecHMAC, 32) == 0) {
        int rtnCode = String((char*)RecRTN).substring(0, 3).toInt();
        Serial.printf("[V] ACK received, RTN: %d\n", rtnCode);
        switch (rtnCode) {
          case RTN_OK:
            ackReceived = true;
            break;
          case RTN_NG:
            printSCR("Msg delivery failed", 0);
            break;
          case RTN_RESYNC:
            Serial.println("[V] Key sync confirmed by remote device");
            // printSCR("Key sync OK", 0);
            break;
          default:
            Serial.printf("[X] Unknown RTN code: %d\n", rtnCode);
            break;
        }
      } else {
        Serial.println("[X] ACK HMAC is NOT valid");
        printSCR("ACK HMAC invalid!", 0);
      }
      free(RecHmacCalc);
      break;
    }

    default:{
      // do something else
      break;
    }
  }
}