# LoRaCore - 3NCRYP2P V2

![Overview picture](https://github.com/sdebby/3NCRYP2P_2/blob/main/media/3ncryp2p%20cover1.png?raw=true)

**When the towers fall, the grid dies, and the satellites stop answering, two of these still talk to each other.**

## Overview

This repository is the forking UPGRADED from the original [sdebby/3NCRYP2P](https://github.com/sdebby/3NCRYP2P) project, refactored under the working name **LoRaCore**.

LoRaCore is the firmware for **3NCRYP2P** (ENCRYPT P2P) - a portable, off-grid, end-to-end encrypted peer-to-peer messaging terminal. It is built around the ESP32-C6, an SSD1306 OLED, a CardKB keyboard, and a 433 MHz LoRa transceiver. No cell tower. No internet. No cloud account. No subscription. Two devices, a pair of public keys, and validated 1.5 km in metropolitan .

---

## In a nutshell

- **Encrypted text messaging over LoRa** at 433 MHz, AES-256 in CTR mode, with HMAC-SHA256 message authentication.
- **Diffie-Hellman key exchange (Curve25519)** so the long-term shared secret is never transmitted, never stored on memory in plaintext, and never reused between sessions.
- **Pairing over ESP-NOW**, a short-range link used only at first introduction. The plaintext public keys and MAC are exposed only to anything within roughly 100 metres line-of-sight in that brief window.
- **At-rest hardening**: the remote device's MAC is persisted only as a salted SHA-256 digest. A flash dump alone does not yield the peer identity or a usable HMAC input.
- **Replay and tamper resistance** through random 16-byte IVs per packet and HMAC verification on every frame, including ACKs.
- **Brute-force lockout**: 10 consecutive HMAC failures permanently lock the device until the operator resyncs keys.
- **Reliable delivery** via a 3-attempt retry loop with 2 second ACK timeout per attempt.
- **Local UI**: 128x64 OLED with 20-line scrollback, full QWERTY input via CardKB, scroll using Up/Down keys, dedicated Fn+K key combination to re-pair.

## MRD (Marketing Requirement)

- Provide a simple, secure, and portable LoRa-based communication terminal.
- Support up to *20* lines of message history on a compact OLED display.
- Enable secure key exchange and encrypted messaging between devices.
- Top notch security using AES-256 in CTR mode, with HMAC-SHA256 message authentication, Diffie-Hellman key exchange (Curve25519) and short range MAC address exchange.
- Allow easy user input and message reading.
- Feedback for system status and errors using debug.
- Military look-alike enclosure 😊.

![Device Overview](https://github.com/sdebby/3NCRYP2P_2/blob/main/media/IMG_0271.jpg?raw=true)
 
 ## Skill Set

- Embedded C++ programming (Arduino/ESP32).
- LoRa wireless communication.
- ESP NOW communication protocol.
- Cryptography (AES256 CTR, Diffie-Hellman, HMAC, Hasing remote MAC address).
- 3D design.
- 3D printing and painting.

## Setup (Software Setup)

**Hardware Requirements:**
| Component | Detail |
|---|---|
| MCU | ESP32-C6 |
| Radio | LoRa SX1278 module, 433 MHz |
| Display | SSD1306 OLED, 128 x 64 px, I2C address 0x3C |
| Keyboard | M5Stack CardKB, I2C address 0x5F |
| Status LED | Builtin LED |
| Enclosure | PETG 3D-printable - design using Fusion 360, source files in `CAD/` |

## BOM (Bill Of Materials):

**For production:**
1. ESP32 C6 dev kit
2. [LoRa RA02](https://docs.ai-thinker.com/_media/lora/docs/c048ps01a1_ra-02_product_specification_v1.1.pdf) (433 MHz module)
3. [SSD1306 OLED - 2.24 inch I2C](https://tcclcd.en.made-in-china.com/product/swOajivDfxpf/China-2-42-Inch-128-64-LCD-SSD1309-Driver-Yellow-Monochrome-OLED-Display.html) (128x64 pixels, I2C)
4. [CardKB keyboard](https://docs.m5stack.com/en/unit/cardkb_1.1) (I2C)
5. [Charger panel](https://www.aliexpress.com/item/1005005037876729.html?spm=a2g0o.order_list.order_list_main.386.6b881802CcDzO8)
6. [433 MHz antenna] (https://he.aliexpress.com/item/1005009824013771.html?spm=a2g0o.order_list.order_list_main.29.77c21802EqfMmP&gatewayAdapt=glo2isr).
7. LiPo battery 850 mAh.
8. On/Off switch.
9. Custom PCB manufacturing using JLCPCB.
10. 3D printing for enclosure.
![3D printing enclosure - from PETG](https://github.com/sdebby/3NCRYP2P_2/blob/main/media/IMG_0247.jpg?raw=true)

## Cryptographic stack

```
Pairing  : Curve25519 ECDH  ->  shared secret  ->  SHA-256  ->  32-byte session key (HashSecret)
Identity : SHA-256( MAC || HashSecret )  -> 32-byte salted MAC digest, persisted in NVS
Confidentiality : AES-256-CTR( HashSecret, random 16-byte IV per packet )
Integrity / authenticity : HMAC-SHA256, key = HashSecret, input = ciphertext || IV || senderMacDigest
ACK integrity : HMAC-SHA256 over the 3-byte return code, same key derivation
Replay defence : fresh random IV per packet, included in HMAC input
Lockout : 10 consecutive HMAC failures sets DevLock = true in NVS, requires re-pair
```

The session key (`HashSecret`) is held in RAM only. It is never written to flash. A power cycle on either device forces a fresh handshake over LoRa using the previously paired peer, and a Fn+K from the operator forces a full re-pair over ESP-NOW.

---

## LoRa packet format

```
[Setup 1B] [PayloadSize 1B] [Payload N B] [IV 16B] [HMAC 32B]
```

| Setup bit | Meaning |
|---|---|
| 0 | Handshake - public key + sender MAC + key-return flag |
| 1 | Test message - decryption sanity check after handshake |
| 2 | Encrypted user message |
| 3 | ACK / NACK - return codes 200 OK, 502 NACK, 500 RESYNC |

---

## File map

| File | Role |
|---|---|
| `LoRaCore.ino` | Main entry. Defines globals, pinout, screen and keyboard setup, the state machine, the `setup()` and `loop()` functions, keyboard handling (Enter, Backspace, Up, Down, Fn+K, Fn+D), the ESP-NOW broadcast peer class, the boot-time decision tree between fresh pairing and resuming an existing pair, and the optional Auto Interval Messages debug sender. |
| `LoRaHelper.ino` | Everything LoRa: radio parameter setup (frequency, bandwidth, spreading factor, coding rate, sync word, TX power), `SendLoRaPayload` for encrypting and transmitting a frame, `SendWithRetry` for the 3-try / 2 s ACK loop, `TransmitLoRaPayload` and `TransmitLoRaACK` and `TransmitLoRaHandshake` for the wire-format writers, and the receive-side `onReceive` switch that dispatches handshake, test, encrypted, and ACK frames - including HMAC verification, lockout counter, and ACK reply. |
| `EncryptHelper.ino` | All cryptographic primitives: Curve25519 keypair generation (`CalcKeys`), peer key processing and shared-secret computation (`computeSharedSecret`, `RemoteKeyReceivedProcess`), session-key derivation via SHA-256 (`CalculHashSecret`), random IV generation, AES-256-CTR encrypt and decrypt, and the raw HMAC-SHA256 calculation. |
| `EspNowHelper.ino` | Short-range pairing transport: Wi-Fi STA setup on channel 6, `ExchangePublicKeysESPNOW` which broadcasts the local public key up to `KeySendTries` times on a 2 s scheduler, and the post-pairing trigger that fires the LoRa test payload from whichever peer has the larger MAC (deterministic tie-break to prevent both devices transmitting simultaneously). |
| `displayHelper.ino` | OLED rendering: display init, `printSCR` which prefixes outgoing (`->`), incoming (`<-`), and system (`* `) messages and word-wraps them into the 20-line scrollback buffer, scroll-up and scroll-down for history navigation, the input-line separator, and the boot logo. |
| `generalHelpers.ino` | Persistence, identity, and integrity glue: NVS preferences setup under namespace `3NCRYPT2P`, legacy migration that strips any plaintext `RemoteMAC` key, the lockout loop that drives the LED red and halts the device, the salted MAC hashing function, the HMAC input builder (ciphertext || IV || macDigest), and the local MAC reader. |
| `Jurnal.md` | Development journal. Chronological design notes from March 2025 onwards: why ESP-NOW was picked over Zigbee, Thread, NFC, BLE and serial; the move from a hardcoded key to Curve25519 ECDH; the move from SHA-256 message digests to HMAC-SHA256; the move from AES-128 to AES-256; the hybrid pair-once-over-ESP-NOW, rekey-each-boot-over-LoRa scheme; bug fixes and feature additions through to spring 2026. |
| `CAD/` | 3D-printable enclosure parts. SolidWorks part file and neutral STEP export for the 2.42-inch OLED carrier. |
| `CLAUDE.md` | Internal project notes used by AI assistants when working on this codebase. Not required for build. |

---

## Build and flash

1. Clone or download this repository.
2. Install the **Arduino IDE** with **ESP32 board support**.
3. Install these libraries through the Library Manager:
   - `LoRa` (Sandeep Mistry)
   - `Adafruit SSD1306` and `Adafruit GFX`
   - `Adafruit NeoPixel`
   - `Crypto` (provides `Curve25519.h`, `SHA256.h`, `AES.h`, `CTR.h`)
   - `ptScheduler`
1. Open `LoRaCore.ino` in the Arduino IDE. The other `.ino` files in this folder are picked up automatically as part of the same sketch.
2. Select board: **ESP32-C6 Dev Module**.
3. Optional: set **Tools -> Core Debug Level -> Debug** to enable the verbose serial console (`Consl = true`) and accept text input over the serial port instead of the keyboard.
4. Compile and upload. Repeat for the second unit.

---

## First-time pairing

1. Power on both devices in the same room.
2. With no remote stored in NVS, each device automatically enters ESP-NOW broadcast mode and exchanges public keys.
3. The device with the larger MAC (deterministic, computed from each side's WiFi STA MAC) sends an AES-256 encrypted test payload over LoRa. The receiver decrypts, verifies HMAC, and the screen confirms `Key exchange process success`.
4. Both devices now show `SYSTEM READY!`. The salted remote-MAC digest is written to NVS. The session key lives only in RAM.

To force a re-pair at any time, press **Fn+K** on the keyboard, then press **ENTER** when the remote operator confirms readiness on their unit.

---

## Daily use

| Action | Key |
|---|---|
| Send the typed message | ENTER |
| Delete the previous character | BACKSPACE |
| Scroll back through history | UP arrow |
| Scroll forward through history | DOWN arrow |
| Force a full re-pair (ESP-NOW) | Fn + K |
| Toggle Auto Interval Messages (debug) | Fn + D |

Outgoing messages are prefixed with `->`. Incoming with `<-`. System messages with `* `. Up to 20 lines of history are kept; six lines are visible at a time on the 128x64 display.

---

## Threat model and known weak points

What this device defends against:

- A passive eavesdropper anywhere along the LoRa path. Without `HashSecret` the traffic is AES-256 ciphertext with random IVs.
- An active attacker injecting forged messages. HMAC-SHA256 over ciphertext, IV, and the sender's salted MAC digest will fail verification, the message is dropped, and the failure counter increments.
- A flash-dump attacker with physical access to one device after the fact. Only the salted MAC digest is on disk; the session key is not.
- Brute-force probing. After 10 consecutive HMAC failures the device permanently locks until re-pair.

What this device does **not** defend against:

- An attacker physically present during the **initial ESP-NOW pairing**. Within roughly 100 metres of the pairing event, the public keys and MAC are observable. This is a short, deliberately bounded window. The original 3NCRYP2P also flagged this as the system's Achilles heel; the move from hardcoded keys to ECDH closed the more serious version of that problem, but pairing-time exposure remains.
- A compromised endpoint. If the attacker holds your device while it is unlocked and paired, they hold the conversation.
- Side-channel attacks against the ESP32-C6 itself.
- Direction-finding. LoRa transmissions are emissions; they can be located by anyone with an SDR and patience. Encryption protects content, not the fact that you are transmitting.

Plan accordingly.

---

## Range

Tested up to **1.5 km** in  urban conditions with the parameters in this firmware. Open terrain, elevation on either end, or a directional antenna will extend that. Heavy concrete, dense forest, or a hill in the middle will shorten it.

---
## Schematics

![Scheme](https://github.com/sdebby/3NCRYP2P/blob/main/media/3ncryp2p%20omni(cardkb%20and%20sdd1306)_bb.jpg?raw=true)

## PCB

* PCB design using KiCad
* 2 layers PCB, black color.
* PCB manufacturing using JLCPCB
![Custom PCB](https://github.com/sdebby/3NCRYP2P_2/blob/main/media/IMG_0277.JPG?raw=true)
you can download the gerber files from /GERBER Library
---

## Hardware assembly:

![PCB Soldering 1](https://github.com/sdebby/3NCRYP2P_2/blob/main/media/IMG_0235.jpg?raw=true)

![PCB Soldering 2](https://github.com/sdebby/3NCRYP2P_2/blob/main/media/IMG_0237.jpg?raw=true)

![Assembly on enclosure 1](https://github.com/sdebby/3NCRYP2P_2/blob/main/media/IMG_0254.jpg?raw=true)

![Assembly on enclosure 2](https://github.com/sdebby/3NCRYP2P_2/blob/main/media/IMG_0255.jpg?raw=true)

### 3D enclosure design

* Using Autodesk Fusion360
* 3D print using Bambulab A1 with PETG filament.
![Fusion360 Design](https://github.com/sdebby/3NCRYP2P_2/blob/main/media/fusion360.jpg?raw=true)
you can download the 3D files from /3D library.

Painting 
![Paint 1](https://github.com/sdebby/3NCRYP2P_2/blob/main/media/IMG_0249.jpg?raw=true)

![Paint 2](https://github.com/sdebby/3NCRYP2P_2/blob/main/media/IMG_0251.jpg?raw=true)

## Range test
**Sender**

![Sender](https://github.com/sdebby/3NCRYP2P_2/blob/main/media/IMG_0273.jpg?raw=true)

---

**Receiver**

![Receiver](https://github.com/sdebby/3NCRYP2P_2/blob/main/media/IMG_0276.jpg?raw=true)


**Non-line of sight distance** 1.5 Km

## Power consumption

![Checking power](https://github.com/sdebby/3NCRYP2P_2/blob/main/media/IMG_0270.jpg?raw=true)

- Battery capacity: 850 mAh
- Idle current: 65 mA (continuous)
- Hourly burst: +162 mA for 25 s (sending 25 messages an hour)
- Startup: 180 mA for 3 s (one-time) (ESP NOW sync)

Average hourly consumption
65 + 1.125 = 66.125 mAh / hour
Usable capacity after startup: 850 − 0.15 = 849.85 mAh
t = 849.85 ÷ 66.125 ≈ 12.85 hours ≈ 12 h 51 min

**Suggestion for optimization**
putting Display on sleep when ideal.
adding LED to display when new message arrives, then the user will press any button to wake up the screen. 

## Video

[![](https://markdown-videos-api.jorgenkh.no/youtube/NzuQNOC-KOk)](https://youtu.be/NzuQNOC-KOk)

## Compare to 3NCRYP2P V1

![Design Compare 1](https://github.com/sdebby/3NCRYP2P_2/blob/main/media/IMG_0261.jpg?raw=true)

![Design Compare 2](https://github.com/sdebby/3NCRYP2P_2/blob/main/media/IMG_0262.jpg?raw=true)

* BETTER ENCRYPTION - breaking V1 Achilles heel - using AES256 asymmetric key encryption.
* HMAC message validation - including device secret MAC address.
* Device MAC address is exchanged in short range only and hashed in device memory.
* A longer (18 cm with 3dbi gain instead of 9 cm) antenna is improving the range from 500 m in urban area to 1.5 Km.
* Military looking device.
* bigger screen (2.42" instead of 1.3")

## Compare to MESHTASTIC project

[Meshtastic](https://meshtastic.org/)
- LoRaCore is a strict **1:1 encrypted terminal**
- Meshtastic is a **many-to-many community mesh**. The table below summarizes the trade-offs.

### Feature comparison

| Area | LoRaCore (3NCRYP2P V2) | Meshtastic |
|---|---|---|
| Network topology | Strict 1:1 paired devices | Multi-hop mesh, many nodes |
| Key exchange | Curve25519 ECDH at pairing, ephemeral per session | Pre-shared channel key (PSK); x25519 PKC added for DMs in v2.5+ |
| Session key storage | RAM only, never on flash | PSK persisted on flash per channel |
| Confidentiality | AES-256-CTR, random 16 B IV per packet | AES-256-CTR (channels), AES-CCM (DMs since v2.5) |
| Message authentication | HMAC-SHA256 on every packet (incl. ACKs) | None on channel messages; AES-CCM tag on DMs only |
| Sender identity binding | Salted SHA-256 MAC digest mixed into HMAC | None - anyone with PSK can impersonate any node |
| Replay defence | Random IV + HMAC per packet (no counter window) | Packet-ID dedup, short window |
| Forward secrecy | Effective (re-pair regenerates keys, key never written to flash) | None - "Harvest now, decrypt later" is acknowledged in their docs |
| Brute-force lockout | Yes - 10 HMAC failures permanently locks NVS | None |
| At-rest hardening | Only salted MAC digest persisted | PSK, node DB, position history persisted |
| Range per hop | ~1.5 km validated urban, 433 MHz | Similar per hop (varies by band/region) |
| Effective reach | Single hop only | Multi-hop, region-wide with enough nodes |
| Frequency band | 433 MHz (fixed at compile time) | 433 / 868 / 915 MHz, region-configurable |
| User interface | Standalone - built-in OLED + CardKB, no phone needed | Phone app (BLE/Wi-Fi) on most devices; few have native KB/screen |
| Hardware support | One purpose-built ESP32-C6 design | Dozens: T-Beam, Heltec V3, T114, RAK WisBlock, Wio Tracker, etc. |
| Attack surface | LoRa + ESP-NOW only, no IP stack, no BLE, no MQTT | LoRa + BLE + Wi-Fi + MQTT + web admin + OTA |
| Code base size | ~6 short `.ino` files, auditable in an afternoon | Large C++ codebase, many subsystems |
| Ecosystem & community | Personal project, single maintainer | Large open-source community, active CVE tracking |
| Power budget | ~66 mA average, ~12.85 h on 850 mAh | Varies; nRF52 boards (e.g. T114) reach ~11 µA deep sleep |
| Group messaging | Not supported by design | Native - channels and groups |
| GPS / telemetry | Not included | Built-in on many boards (position, battery, env sensors) |
| Setup effort | Two pre-paired units, no config | Phone pairing, channel keys, region selection |

## Next steps
[ ] Increase range with a directional antenna ,higher gain or mesh communication.

[ ] Increase partners.

## A note for whoever finds this later

If you are reading this and the rest of the network is gone, the recipe is in the files.

**Do not panic !**