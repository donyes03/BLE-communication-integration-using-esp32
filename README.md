# BLE Communication Integration in Automotive Systems Using ESP32

A two-phase BLE communication project developed as a prototype for an automotive **Digital Key** use case.

The project investigates Bluetooth Low Energy communication between an **ESP32** and a **PC**, including BLE connection establishment, security, GATT service/characteristic handling, ATT Read/Write operations, notifications, and application-level vehicle lock/unlock simulation.

> **Project scope:** This project is a BLE/GATT prototype inspired by automotive Digital Key communication flows. 

---

## Project Overview

The project is divided into **two phases**.

### Phase 1 — ESP32 Server / PC Client

In the first phase:

- **ESP32 acts as the BLE Server**
- **PC acts as the BLE Client**

The objective of this phase was to understand the fundamentals of BLE communication and develop the initial server-side implementation on the ESP32.

This phase includes several experimental implementations covering:

- BLE advertising
- BLE connection establishment
- GATT services and characteristics
- Read / Write operations
- Notifications
- BLE security
- Encryption experiments
- Multiple connection experiments

The corresponding implementations are available under:

```text
Code/ESP_Server/
````

Development versions include:

```text
First_test/
Server_example/
Server_WithEncryption/
led_control/
multi-connection_PIN/
simple_multi_connection/
```

---

# Phase 2 — PC Server / ESP32 Client

The second phase is the **main and most important phase of the project**.

In this configuration:

* **PC acts as the BLE Server**
* **ESP32 acts as the BLE Client**
  
The PC server is implemented in **Python** using the **Bless** BLE library.

The ESP32 client is implemented in **C++ using Arduino and the ESP32 BLE libraries**.

The objective is to simulate an automotive vehicle-side BLE service and reproduce a simplified Digital Key communication flow.

The PC creates the GATT service and characteristic.

The ESP32 scans for the PC, establishes the BLE connection, discovers the GATT structure, and performs the application communication.

The main implementation is located under:

```text
Code/Digital_Key_PC_Server/Client_Final/
```

with the ESP32 firmware:

```text
Client_Final.ino
```

---

## Phase 2 Architecture

```text
              Bluetooth Low Energy
        ┌────────────────────────────┐
        │                            │
        ▼                            ▼

┌───────────────────┐        ┌───────────────────┐
│       ESP32       │        │        PC         │
│                   │        │                   │
│   BLE CLIENT      │◄──────►│    BLE SERVER     │
│                   │        │                   │
│ Arduino / C++     │        │ Python / Bless    │
│                   │        │                   │
└───────────────────┘        └───────────────────┘
```

The ESP32 acts as the client and communicates with the GATT server running on the PC.

---

# Main Objectives

The project focuses on the following objectives:

* Establish BLE communication between an ESP32 and a PC.
* Understand the main BLE communication layers.
* Implement GATT services and characteristics.
* Implement BLE security and passkey-based authentication.
* Perform GATT service and characteristic discovery.
* Implement ATT Read and Write procedures.
* Implement BLE notifications.
* Simulate vehicle lock/unlock behavior.
* Create a detailed BLE communication logger.

---

# BLE Protocol Stack

The project mainly uses the following BLE layers:

### GAP

GAP is responsible for advertising, device discovery, and BLE connection procedures.

### SMP

SMP is responsible for BLE pairing and security procedures.

### GATT

GATT defines the services and characteristics used by the application.

### ATT

ATT provides attribute operations such as:

* Read Request
* Read Response
* Write Request
* Write Response
* Notifications

### L2CAP

L2CAP transports ATT and SMP data through their respective fixed channels.

---

# GATT Configuration

The current prototype uses one custom service and one custom characteristic.

| Element             | Value                                  |
| ------------------- | -------------------------------------- |
| Service UUID        | `b4250400-3446-4654-b59a-1c888d447433` |
| Characteristic UUID | `b4250401-3446-4654-b59a-1c888d447433` |
| Diagnostic Handle   | `0x002A`                               |
| Properties          | Read, Write, Notify                    |
| Lock Command        | `TOGGLE_LOCK`                          |

The ESP32 uses the UUIDs to identify the expected service and characteristic during GATT discovery.

The characteristic handle `0x002A` is intentionally hardcoded in this prototype.

---

# Communication Sequence

The main communication sequence is:

```text
PC Server
    │
    │ BLE Advertising
    ▼
ESP32 Client
    │
    │ Device Discovery
    ▼
PC Server
    │
    │ BLE Connection
    ▼
ESP32 Client
    │
    │ BLE Security / Pairing
    ▼
ESP32 Client
    │
    │ GATT Service Discovery
    ▼
PC Server
    │
    │ Characteristic Discovery
    ▼
ESP32 Client
    │
    │ Enable Notifications
    ▼
PC Server
```

After the BLE connection and GATT setup are complete, the application communication starts.

---

# BLE Security

The ESP32 is configured for BLE security using:

* Passkey pairing
* MITM protection
* Secure Connections
* BLE SMP authentication

The ESP32 security callbacks provide events such as:

* Passkey request
* Passkey notification
* Authentication success
* Authentication failure
* Security request

The project also contains earlier experiments related to encrypted characteristics and BLE security.

---

# PC Security Limitation

The PC implementation relies on the **Windows Bluetooth stack** through Bless.

Windows does not expose all low-level SMP events directly to the Python application.

Therefore:

* The PC cannot provide a complete raw SMP trace.
* Some authentication information is shown as an application-level diagnostic message.
* The ESP32 provides more detailed security callbacks than the PC.
* Passkey interaction is limited by the Windows BLE API and stack used by the application.

The PC authentication-success message should therefore be considered a **diagnostic indication**, not a raw SMP packet capture.

---

# GATT Discovery

After the BLE connection is established, the ESP32 searches for the configured service.

```text
Service UUID:
b4250400-3446-4654-b59a-1c888d447433
```

It then searches for the characteristic:

```text
Characteristic UUID:
b4250401-3446-4654-b59a-1c888d447433
```

The discovery sequence is:

```text
BLE Connection
      ↓
Service Discovery
      ↓
Service Found
      ↓
Characteristic Discovery
      ↓
Characteristic Found
```

Only after the required GATT elements are found does the ESP32 start the application communication.

---

# ATT Read Operation

The ESP32 sends a Read Request to the diagnostic characteristic.

```text
ATT_READ_REQ
Opcode: 0x0A
Handle: 0x002A
```

The PC returns:

```text
ATT_READ_RSP
Opcode: 0x0B
```

with the application value:

```text
Connection with PC Verified
```

### Sequence

```text
ESP32 Client                              PC Server
     │                                        │
     │────────── ATT_READ_REQ ───────────────►│
     │                                        │
     │                                        │
     │◄───────── ATT_READ_RSP ────────────────│
     │     Connection with PC Verified        │
     │                                        │
```

---

# ATT Write Operation

The ESP32 sends the application command:

```text
TOGGLE_LOCK
```

The corresponding Write Request is:

```text
ATT_WRITE_REQ
Opcode: 0x12
Handle: 0x002A
Value: TOGGLE_LOCK
```

The PC returns:

```text
ATT_WRITE_RSP
Opcode: 0x13
```

### Sequence

```text
ESP32 Client                              PC Server
     │                                        │
     │───────── ATT_WRITE_REQ ───────────────►│
     │           TOGGLE_LOCK                  │
     │                                        │
     │◄──────── ATT_WRITE_RSP ────────────────│
     │                                        │
     │                                        │
     │◄───── ATT_HANDLE_VALUE_NTF ────────────│
     │       Car Unlocked/ Locked             │
     │                                        │
     │                                        │
```

# Vehicle Lock Simulation

The PC maintains a simple vehicle lock state.

Initial state:

```text
Car Locked
```

When the ESP32 sends:

```text
TOGGLE_LOCK
```

the PC toggles the state.

Example:

```text
Car Locked
     │
     │ TOGGLE_LOCK
     ▼
Car Unlocked
```

A second command changes it back:

```text
Car Unlocked
     │
     │ TOGGLE_LOCK
     ▼
Car Locked
```

The new state is then sent to the ESP32 through a BLE notification.

---

# Notification

The PC sends the new vehicle state using:

```text
ATT_HANDLE_VALUE_NTF
Opcode: 0x1B
Handle: 0x002A
Value: Car Locked / Car Unlocked
```

Example:

```text
1B 2A 00 43 61 72 20 55 6E 6C 6F 63 6B 65 64
```

which represents:

```text
Car Unlocked
```

### Communication sequence

```text
ESP32 Client                              PC Server
     │                                        │
     │──────── ATT_WRITE_REQ ────────────────►│
     │          TOGGLE_LOCK                   │
     │                                        │
     │◄─────── ATT_WRITE_RSP ─────────────────│
     │          13                            │
     │                                        │
     │◄─────── ATT_HANDLE_VALUE_NTF ──────────│
     │         Car Unlocked/ Locked           │
     │                                        │
     │                                        │
```

The Write Response is generated before the vehicle-status notification in the application flow.

---

# Communication Logger

One of the main goals of Phase 2 is the development of a protocol-level BLE logger.

The logger displays:

```text
TIME
DEVICE
DIR
LAYER
EVENT
PDU HEX
PAYLOAD ASCII
```

Example layers include:

```text
GAP
SMP
GATT
L2CAP
ATT
```

Example:

```text
TIME          DEVICE  DIR  LAYER  EVENT                               
14:06:23.633  PC      RX   L2CAP  DATA                                
14:06:23.633  PC      RX   ATT    ATT_WRITE_REQ                      
14:06:23.633  PC      TX   L2CAP  DATA                                
14:06:23.633  PC      TX   ATT    ATT_WRITE_RSP                      
14:06:23.634  PC      TX   L2CAP  DATA                               
14:06:23.634  PC      TX   ATT    ATT_HANDLE_VALUE_NTF                
```

The logger provides a readable view of the BLE communication on both sides.

---

# L2CAP Logging

The project reconstructs L2CAP information for ATT communication.

The fixed ATT L2CAP CID used in the logger is:

```text
0x0004
```

Example Read Request:

```text
03 00 04 00 0A 2A 00
```

This can be interpreted as:

```text
03 00    L2CAP Length
04 00    CID = 0x0004
0A       ATT_READ_REQ
2A 00    Attribute Handle
```

Example Notification:

```text
0F 00 04 00 1B 2A 00 ...
```

The logger displays the complete reconstructed L2CAP and ATT data whenever available.

---

# SMP Logging

Security-related events are logged under:

```text
SMP
```

Examples include:

```text
PASSKEY_REQUEST
PASSKEY_NOTIFY
AUTHENTICATION
SECURITY_REQUEST
CONFIRM_PIN
```

The ESP32 security callbacks do not provide the raw over-the-air SMP PDU bytes used by the controller.

Therefore, no SMP PDU data is fabricated in the logger.

---

# Connection Monitoring

The ESP32 periodically sends a small keep-alive value:

```text
00
```

The value is used to maintain connection activity and is hidden from the main application logger.

The PC monitors GATT activity using a watchdog.

If no GATT activity is received for the configured timeout, the PC reports:

```text
[BLE] CONNECTION LOST
```

and returns to the waiting state.

This is an **application-level inactivity watchdog**, not a raw BLE controller-level disconnect detector.

---

# Repository Structure

```text
BLE-communication-integration-using-esp32/
│
├── Code/
│   │
│   ├── Digital_Key_PC_Server/
│   │   └── Client_Final/
│   │       └── Client_Final.ino
│   │
│   └── ESP_Server/
│       ├── First_test/
│       ├── Server_WithEncryption/
│       ├── Server_example/
│       ├── led_control/
│       ├── multi-connection_PIN/
│       └── simple_multi_connection/
│
├── Demo/
│   ├── esp_pc_pairing.mp4
│   ├── esp_read.mp4
│   ├── led_control.mp4
│   ├── led_control_hard.mp4
│   └── simple_multi_connection.mp4
│
├── Documentation/
│   ├── Documentation_Esp_Server.pdf
│   └── Documentation_PC_Server1.pdf
│
├── Presentation/
│   └── BLE2.pptx
│
└── Support/
    ├── BLE.docx
    ├── BLE_2.docx
    ├── BLE_ATT_Opcodes.docx
    ├── Bluetooth_Low_Energy.pdf
    └── ble.pdf
```

---

# Hardware

## Main Hardware

* ESP32 Pico-D4 / compatible ESP32 development board
* USB cable
* Push button
* LEDs
* Resistors
* Bluetooth-capable PC

---

# Software Requirements

## ESP32

* Arduino IDE
* ESP32 board support package
* ESP32 BLE libraries
* Serial Monitor

## PC

* Windows
* Python 3
* Bluetooth support
* Bless BLE library

---

# Installation

## ESP32

Install the ESP32 board support package in Arduino IDE.

Open:

```text
Code/Digital_Key_PC_Server/Client_Final/Client_Final.ino
```

Select the appropriate ESP32 board and COM port.

Upload the firmware and open the Serial Monitor at:

```text
115200 baud
```

---

## PC

Install Python 3 and the required BLE library.

Install Bless:

```bash
pip install bless
```

Then run the PC BLE server from the corresponding project directory.

---

# Running Phase 2

## Step 1 — Start the PC Server

Start the Python BLE server.

The PC creates the GATT server and waits for the ESP32 client.

---

## Step 2 — Start the ESP32

Flash the ESP32 client firmware.

Open the Serial Monitor at:

```text
115200 baud
```

---

## Step 3 — BLE Discovery

The ESP32 scans for the PC server.

```text
Scanning
   ↓
PC discovered
```

---

## Step 4 — Connection

The ESP32 connects to the PC.

```text
BLE Connection Established
```

---

## Step 5 — Security

BLE security and authentication procedures are performed.

---

## Step 6 — GATT Discovery

The ESP32 discovers:

```text
Service
   ↓
Characteristic
```

---

## Step 7 — Read Test

The ESP32 sends a Read Request.

Expected response:

```text
Connection with PC Verified
```

---

## Step 8 — Lock Test

The ESP32 sends:

```text
TOGGLE_LOCK
```

Expected notification:

```text
Car Unlocked
```

Sending the command again produces:

```text
Car Locked
```

---

# Limitations

## Windows Bluetooth Stack

The PC implementation relies on the Windows Bluetooth stack through Bless.

Windows does not expose all low-level BLE and SMP events to the Python application.

As a result:

* Complete raw SMP packets cannot be logged by the PC application.
* The PC cannot provide the same security-event visibility as the ESP32.
* Some authentication information is represented through application-level diagnostic messages.
* Passkey handling depends on the Windows Bluetooth interface and its available functionality.

## Authentication Visibility

The ESP32 provides direct BLE security callbacks.

On the PC side, the application does not have direct access to the complete raw SMP authentication sequence.

Therefore, the displayed PC authentication-success message is a **diagnostic/simulated indication**, not a raw captured SMP authentication packet.

## Protocol Logging

The displayed L2CAP and ATT frames are reconstructed from the high-level GATT API.

The logger is therefore:

> **not a raw over-the-air BLE packet sniffer.**

For a real air-interface capture, a dedicated BLE sniffer or controller-level capture would be required.

## Hardcoded Attribute Handle

The diagnostic characteristic handle:

```text
0x002A
```

is intentionally hardcoded in this prototype.

## Vehicle Simulation

The lock/unlock behavior is an application-level simulation.

It does not control a real vehicle.

## Digital Key Scope

This project is a BLE communication prototype inspired by automotive Digital Key concepts.

It does not implement the complete CCC Digital Key ecosystem, including:

* Complete credential management
* Production vehicle authentication architecture
* Full automotive security architecture
* UWB ranging
* NFC interaction
* Production vehicle integration

---

# Documentation

Detailed documentation is available under:

```text
Documentation/
```

The documentation covers both main phases:

* **ESP32 Server / PC Client**
* **PC Server / ESP32 Client**

---

# Presentation

The project presentation is available under:

```text
Presentation/
```

Main presentation:

```text
BLE2.pptx
```

---

# Demo Videos

Demonstration videos are available under:

```text
Demo/
```

They include demonstrations related to:

* ESP32 ↔ PC pairing
* ESP32 Read operation
* LED/lock control
* Hardcoded lock control
* Multi-connection experiments

---

# Support Material

Additional BLE and ATT reference material is available under:

```text
Support/
```

This directory contains:

* BLE documentation
* ATT opcode references
* Bluetooth Low Energy references
* Supporting technical material

---

# Project Status

## Phase 1

**Completed**

ESP32-server / PC-client BLE experiments were implemented to establish the initial BLE foundation and investigate:

* BLE server configuration
* Advertising
* Services
* Characteristics
* Security
* Encryption
* Multiple connections

---

## Phase 2

**Main implementation — Functional**

The current prototype demonstrates:

* BLE advertising and discovery
* BLE connection establishment
* BLE security/authentication
* GATT service discovery
* GATT characteristic discovery
* ATT Read Request / Response
* ATT Write Request / Response
* BLE notifications
* Vehicle lock/unlock simulation
* Connection monitoring
* Protocol-level diagnostic logging

---

# Conclusion

This project provides a practical BLE communication prototype for an automotive-oriented Digital Key use case.

The two-phase approach allowed BLE communication to be explored from both perspectives:

```text
Phase 1
ESP32 Server  ↔  PC Client
```

and:

```text
Phase 2
PC Server     ↔  ESP32 Client
              ↑
        Main implementation
```

Phase 2 extends the initial BLE experiments into a more complete application-oriented prototype with:

* BLE security
* GATT discovery
* ATT communication
* Notifications
* Vehicle-state simulation
* Connection monitoring
* Detailed diagnostic logging

---

# Author

**Donyes Hsairi - Electronic Communication Systems Engineering Student at ENET'Com**


