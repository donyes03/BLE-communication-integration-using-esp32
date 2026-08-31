#include <Arduino.h>

#include <BLEDevice.h>
#include <esp_system.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEClient.h>
#include <BLESecurity.h>



const char* getResetReasonText(esp_reset_reason_t reason){
    switch (reason)
    {
        case ESP_RST_UNKNOWN:
            return "UNKNOWN";

        case ESP_RST_POWERON:
            return "POWERON_RESET";

        case ESP_RST_EXT:
            return "EXTERNAL_RESET";

        case ESP_RST_SW:
            return "SOFTWARE_RESET";

        case ESP_RST_PANIC:
            return "PANIC_RESET";

        case ESP_RST_INT_WDT:
            return "INT_WDT_RESET";

        case ESP_RST_TASK_WDT:
            return "TASK_WDT_RESET";

        case ESP_RST_WDT:
            return "WDT_RESET";

        case ESP_RST_DEEPSLEEP:
            return "DEEPSLEEP_RESET";

        case ESP_RST_BROWNOUT:
            return "BROWNOUT_RESET";

        case ESP_RST_SDIO:
            return "SDIO_RESET";

        default:
            return "OTHER";
    }}


void printResetReason(){
    esp_reset_reason_t reason =
        esp_reset_reason();

    Serial.printf(
        "[SYSTEM] Reset reason: %s (%d)\n",
        getResetReasonText(reason),
        static_cast<int>(reason)
    );}


#define SERVICE_UUID "b4250400-3446-4654-b59a-1c888d447433"
#define CHARACTERISTIC_UUID "b4250401-3446-4654-b59a-1c888d447433"

#define DEVICE_NAME "ESP32_Client"

#define DIAGNOSTIC_HANDLE 0x002A

#define BOOT_BUTTON_PIN 0
#define WRITE_BUTTON_PIN 4
#define LED_PIN 5

#define BLE_PASSKEY 583214


BLEClient* pClient = nullptr;

BLERemoteCharacteristic*
    pRemoteCharacteristic = nullptr;

BLEAdvertisedDevice*
    myDevice = nullptr;

BLESecurity*
    pSecurity = nullptr;


// ============================================================
// CONNECTION / APPLICATION FLAGS
// ============================================================

volatile bool doConnect = false;
volatile bool connected = false;
volatile bool authenticated = false;
volatile bool securityFailed = false;


// ============================================================
// TIMER
// ============================================================

unsigned long last_ping = 0;


// ============================================================
// PAYLOAD → HEX
// ============================================================

String payloadToHex(
    const uint8_t* data,
    size_t length
)
{
    if (
        data == nullptr ||
        length == 0
    )
    {
        return "-";
    }

    String result;

    for (
        size_t i = 0;
        i < length;
        i++
    )
    {
        if (i > 0)
            result += " ";

        char buffer[4];

        sprintf(
            buffer,
            "%02X",
            data[i]
        );

        result += buffer;
    }

    return result;
}


// ============================================================
// PAYLOAD → ASCII
// ============================================================

String payloadToAscii(
    const uint8_t* data,
    size_t length
)
{
    if (
        data == nullptr ||
        length == 0
    )
    {
        return "-";
    }

    String result;

    for (
        size_t i = 0;
        i < length;
        i++
    )
    {
        if (
            data[i] >= 32 &&
            data[i] <= 126
        )
        {
            result += (char)data[i];
        }
        else
        {
            result += ".";
        }
    }

    return result;
}


// ============================================================
// ATT PDU BUILDERS
// ============================================================

size_t buildReadRequestPdu(
    uint8_t* pdu,
    uint16_t handle
)
{
    pdu[0] = 0x0A;

    pdu[1] = handle & 0xFF;

    pdu[2] = (handle >> 8) & 0xFF;

    return 3;
}


size_t buildReadResponsePdu(
    uint8_t* pdu,
    const uint8_t* value,
    size_t valueLength
)
{
    pdu[0] = 0x0B;

    memcpy(
        &pdu[1],
        value,
        valueLength
    );

    return valueLength + 1;
}


size_t buildWriteRequestPdu(
    uint8_t* pdu,
    uint16_t handle,
    const uint8_t* value,
    size_t valueLength
)
{
    pdu[0] = 0x12;

    pdu[1] = handle & 0xFF;

    pdu[2] = (handle >> 8) & 0xFF;

    memcpy(
        &pdu[3],
        value,
        valueLength
    );

    return valueLength + 3;
}


size_t buildWriteResponsePdu(
    uint8_t* pdu
)
{
    pdu[0] = 0x13;

    return 1;
}


size_t buildNotificationPdu(
    uint8_t* pdu,
    uint16_t handle,
    const uint8_t* value,
    size_t valueLength
)
{
    pdu[0] = 0x1B;

    pdu[1] = handle & 0xFF;

    pdu[2] = (handle >> 8) & 0xFF;

    memcpy(
        &pdu[3],
        value,
        valueLength
    );

    return valueLength + 3;
}


/* ============================================================
 * L2CAP
 *
 * Diagnostic reconstruction only: the GATT API provides the ATT
 * operation, so the logger shows Length + CID + ATT PDU.
 * No Link Layer fields are invented.
 * ============================================================ */


#define L2CAP_ATT_CID 0x0004


size_t buildL2CAPPdu(
    uint8_t* pdu,
    const uint8_t* attPdu,
    size_t attLength,
    uint16_t cid = L2CAP_ATT_CID
)
{
    // L2CAP Basic Frame:
    // Length: 2 bytes, little-endian
    // CID:    2 bytes, little-endian
    // Length excludes these 4 header bytes.

    pdu[0] = attLength & 0xFF;
    pdu[1] = (attLength >> 8) & 0xFF;

    pdu[2] = cid & 0xFF;
    pdu[3] = (cid >> 8) & 0xFF;

    memcpy(
        &pdu[4],
        attPdu,
        attLength
    );

    return attLength + 4;
}


// ============================================================
// ATT OPCODE NAMES
// ============================================================

String getAttCommandName(
    uint8_t opcode
)
{
    switch (opcode)
    {
        case 0x01:
            return "ATT_ERROR_RSP";

        case 0x02:
            return "ATT_EXCHANGE_MTU_REQ";

        case 0x03:
            return "ATT_EXCHANGE_MTU_RSP";

        case 0x04:
            return "ATT_FIND_INFORMATION_REQ";

        case 0x05:
            return "ATT_FIND_INFORMATION_RSP";

        case 0x08:
            return "ATT_READ_BY_TYPE_REQ";

        case 0x09:
            return "ATT_READ_BY_TYPE_RSP";

        case 0x0A:
            return "ATT_READ_REQ";

        case 0x0B:
            return "ATT_READ_RSP";

        case 0x12:
            return "ATT_WRITE_REQ";

        case 0x13:
            return "ATT_WRITE_RSP";

        case 0x1B:
            return "ATT_HANDLE_VALUE_NTF";

        case 0x1D:
            return "ATT_HANDLE_VALUE_IND";

        case 0x1E:
            return "ATT_HANDLE_VALUE_CFM";

        case 0x52:
            return "ATT_WRITE_CMD";

        default:
        {
            char buffer[40];

            sprintf(
                buffer,
                "UNKNOWN_OPCODE (0x%02X)",
                opcode
            );

            return String(buffer);
        }
    }
}


// ============================================================
// LOGGER
// ============================================================

void printLegend()
{
    Serial.println();

    Serial.println(
        "TIME         DEVICE  DIR   LAYER   EVENT                             PDU HEX                                                     PAYLOAD ASCII"
    );

    Serial.println(
        "------------------------------------------------------------------------------------------------------------------------------------------------"
    );
}


void logEvent(
    const char* direction,
    const char* layer,
    const String& event,
    const uint8_t* pdu,
    size_t pduLength,
    const uint8_t* payload,
    size_t payloadLength,
    uint16_t handle,
    bool hasHandle,
    const String& description,
    bool fullPdu = false
)
{
    unsigned long ms =
        millis() % 1000;

    char timeString[20];

    sprintf(
        timeString,
        "+%04lu.%03lu",
        millis() / 1000,
        ms
    );

    String eventText =
        event;

    if (
        description.length() > 0
    )
    {
        eventText += " - ";

        eventText += description;
    }

    String pduHex =
        payloadToHex(
            pdu,
            pduLength
        );

    String ascii =
        payloadToAscii(
            payload,
            payloadLength
        );

    bool showFullPdu =
        fullPdu ||
        (layer == "L2CAP" && event == "DATA") ||
        event == "ATT_READ_RSP" ||
        event == "ATT_HANDLE_VALUE_NTF";

    if (!showFullPdu)
    {
        if (pduHex.length() > 45)
        {
            pduHex =
                pduHex.substring(0, 42)
                + "...";
        }

        if (ascii.length() > 30)
        {
            ascii =
                ascii.substring(0, 27)
                + "...";
        }
    }

    Serial.printf(
        "%-12s  %-6s  %-3s  %-5s  %-38s  %-53s  %s\n",

        timeString,

        "ESP32",

        direction,

        layer,

        eventText.c_str(),

        pduHex.c_str(),

        ascii.c_str()
    );

    Serial.flush();
}


// ============================================================
// SMP
//
// LE fixed L2CAP channel:
//   CID 0x0006 = Security Manager Protocol (SMP)
//
// The Arduino BLE security callbacks expose security events,
// but they do NOT provide the raw on-air SMP PDU bytes.
// Therefore SMP events below intentionally have no fabricated
// PDU HEX.
// ============================================================

#define L2CAP_SMP_CID 0x0006


String getSmpEventName(
    const char* eventName
)
{
    return String(eventName);
}


void logSmpEvent(
    const String& eventName,
    const String& detail
)
{
    logEvent(
        "INT",
        "SMP",
        eventName,
        nullptr,
        0,
        nullptr,
        0,
        0,
        false,
        "CID 0x0006  " + detail
    );
}


const char* getSmpFailureDescription(
    uint8_t reason
)
{
    switch (reason)
    {
        // Bluetooth Core SMP reason codes
        case ESP_AUTH_SMP_PASSKEY_FAIL:
            return "PASSKEY_FAIL - User input of passkey failed";

        case ESP_AUTH_SMP_OOB_FAIL:
            return "OOB_FAIL - OOB data is not available";

        case ESP_AUTH_SMP_PAIR_AUTH_FAIL:
            return "PAIR_AUTH_FAIL - Authentication requirements cannot be met";

        case ESP_AUTH_SMP_CONFIRM_VALUE_FAIL:
            return "CONFIRM_VALUE_FAIL - Confirm value mismatch";

        case ESP_AUTH_SMP_PAIR_NOT_SUPPORT:
            return "PAIR_NOT_SUPPORT - Pairing is not supported";

        case ESP_AUTH_SMP_ENC_KEY_SIZE:
            return "ENC_KEY_SIZE - Encryption key size is not long enough";

        case ESP_AUTH_SMP_INVALID_CMD:
            return "INVALID_CMD - Unsupported SMP command";

        case ESP_AUTH_SMP_UNKNOWN_ERR:
            return "UNKNOWN_ERR - Unspecified pairing failure";

        case ESP_AUTH_SMP_REPEATED_ATTEMPT:
            return "REPEATED_ATTEMPT - Pairing/authentication disallowed";

        case ESP_AUTH_SMP_INVALID_PARAMETERS:
            return "INVALID_PARAMETERS - Invalid SMP command length/parameter";

        case ESP_AUTH_SMP_DHKEY_CHK_FAIL:
            return "DHKEY_CHK_FAIL - DHKey Check mismatch";

        case ESP_AUTH_SMP_NUM_COMP_FAIL:
            return "NUM_COMP_FAIL - Numeric comparison mismatch";

        // Bluedroid host-specific reasons
        case ESP_AUTH_SMP_INTERNAL_ERR:
            return "INTERNAL_ERR - Internal pairing error";

        case ESP_AUTH_SMP_UNKNOWN_IO:
            return "UNKNOWN_IO - Unknown IO capability";

        case ESP_AUTH_SMP_INIT_FAIL:
            return "INIT_FAIL - SMP pairing initiation failed";

        case ESP_AUTH_SMP_CONFIRM_FAIL:
            return "CONFIRM_FAIL - Confirm value mismatch";

        case ESP_AUTH_SMP_BUSY:
            return "BUSY - Security request already in progress";

        case ESP_AUTH_SMP_ENC_FAIL:
            return "ENC_FAIL - Controller failed to start encryption";

        case ESP_AUTH_SMP_STARTED:
            return "STARTED - SMP pairing process started";

        case ESP_AUTH_SMP_RSP_TIMEOUT:
            return "RSP_TIMEOUT - No SMP command received before timeout";

        case ESP_AUTH_SMP_DIV_NOT_AVAIL:
            return "DIV_NOT_AVAIL - Encrypted Diversifier unavailable";

        case ESP_AUTH_SMP_UNSPEC_ERR:
            return "UNSPEC_ERR - Unspecified failure";

        case ESP_AUTH_SMP_CONN_TOUT:
            return "CONN_TOUT - Pairing connection timeout";

        default:
            return "UNKNOWN - Failure code not mapped by ESP-IDF";
    }
}


// ============================================================
// SECURITY CALLBACKS
// ============================================================

class MySecurityCallbacks :
    public BLESecurityCallbacks
{
public:

    uint32_t onPassKeyRequest() override
    {
        Serial.println();

        Serial.println(
            "[SMP] Passkey requested."
        );

        Serial.printf(
            "[SMP] Passkey = %06u\n",
            BLE_PASSKEY
        );

        logSmpEvent(
            "PASSKEY_REQUEST",
            "Static passkey requested"
        );

        return BLE_PASSKEY;
    }


    void onPassKeyNotify(
        uint32_t pass_key
    ) override
    {
        Serial.printf(
            "[SMP] Passkey notification: %06u\n",
            pass_key
        );

        logSmpEvent(
            "PASSKEY_NOTIFY",
            "Passkey notification received"
        );
    }


    bool onSecurityRequest() override
    {
        Serial.println(
            "[SMP] Security request received."
        );

        logSmpEvent(
            "SECURITY_REQUEST",
            "Security request accepted"
        );

        return true;
    }


    bool onConfirmPIN(
        uint32_t pin
    ) override
    {
        Serial.printf(
            "[SMP] PIN confirmation: %06u\n",
            pin
        );

        logSmpEvent(
            "CONFIRM_PIN",
            "PIN confirmation requested"
        );

        return (
            pin == BLE_PASSKEY
        );
    }


#if defined(CONFIG_BLUEDROID_ENABLED)

        void onAuthenticationComplete(
        esp_ble_auth_cmpl_t auth
    ) override
    {
        if (auth.success)
        {
            authenticated = true;
            securityFailed = false;

            logSmpEvent(
                "AUTHENTICATION",
                "SUCCESS"
            );

            Serial.println();
            Serial.println("[SMP] AUTHENTICATION SUCCESS");
            Serial.println("[SMP] BLE security established.");
            Serial.println();

            // Defer the application-level confirmation until the
            // BLE security callback has returned. Calling GATT write
            // operations directly from the SMP callback can be unsafe.
        }
        else
        {
            authenticated = false;
            securityFailed = true;

            uint8_t reason =
                static_cast<uint8_t>(auth.fail_reason);

            const char* reasonText =
                getSmpFailureDescription(reason);

            logSmpEvent(
                "AUTHENTICATION",
                String("FAILED - code ") +
                String(reason) +
                " (0x" +
                String(reason, HEX) +
                ")"
            );

            Serial.println();
            Serial.println(
                "============================================================"
            );

            Serial.println(
                "[SMP] AUTHENTICATION FAILED"
            );

            Serial.printf(
                "[SMP] Failure code : %u (0x%02X)\n",
                reason,
                reason
            );

            Serial.printf(
                "[SMP] Failure type : %s\n",
                reasonText
            );

            Serial.println(
                "============================================================"
            );
        }
    }

#endif
};


// ============================================================
// NOTIFICATION CALLBACK
// ============================================================

static void notifyCallback(
    BLERemoteCharacteristic* characteristic,
    uint8_t* pData,
    size_t length,
    bool isNotify
)
{
    if (
        pData == nullptr ||
        length == 0
    )
    {
        return;
    }

    uint8_t pdu[260];

    size_t pduLength =
        buildNotificationPdu(
            pdu,
            DIAGNOSTIC_HANDLE,
            pData,
            length
        );

    uint8_t l2capPdu[264];

    size_t l2capLength =
        buildL2CAPPdu(
            l2capPdu,
            pdu,
            pduLength
        );

    logEvent(
        "RX",
        "L2CAP",
        "DATA",
        l2capPdu,
        l2capLength,
        pData,
        length,
        0,
        false,
        "Length=" + String(pduLength) + ", CID=0x0004",
        true
    );

    logEvent(
        "RX",
        "ATT",
        getAttCommandName(0x1B),
        pdu,
        pduLength,
        pData,
        length,
        DIAGNOSTIC_HANDLE,
        true,
        "Notification",
        true
    );

    String message =
        payloadToAscii(
            pData,
            length
        );


    if (
        message == "Car Unlocked"
    )
    {
        digitalWrite(
            LED_PIN,
            HIGH
        );
    }
    else if (
        message == "Car Locked"
    )
    {
        digitalWrite(
            LED_PIN,
            LOW
        );
    }

    Serial.println();
}


// ============================================================
// ADVERTISEMENT CALLBACK
// ============================================================

class MyAdvertisedDeviceCallbacks :
    public BLEAdvertisedDeviceCallbacks
{
public:

    void onResult(
        BLEAdvertisedDevice advertisedDevice
    )
    {
        if (
            advertisedDevice.haveServiceUUID()
            &&
            advertisedDevice.isAdvertisingService(
                BLEUUID(SERVICE_UUID)
            )
        )
        {
            BLEDevice::getScan()->stop();

            Serial.println();

            Serial.println(
                "[GAP] PC_server discovered."
            );

            logEvent(
                "RX",
                "GAP",
                "ADV_IND",
                nullptr,
                0,
                nullptr,
                0,
                0,
                false,
                "PC_server advertising detected"
            );

            if (myDevice != nullptr)
            {
                delete myDevice;
            }

            myDevice =
                new BLEAdvertisedDevice(
                    advertisedDevice
                );

            doConnect = true;
        }
    }
};


// ============================================================
// CLIENT CALLBACKS
// ============================================================

class MyClientCallback :
    public BLEClientCallbacks
{
public:

    void onConnect(BLEClient* client)
    {
        connected = true;
        authenticated = false;
        securityFailed = false;
        Serial.println();
        Serial.println("[BLE] CONNECTION ESTABLISHED");
        Serial.println("[BLE] Connected to PC_server.");
        Serial.println();
}

    void onDisconnect(BLEClient* client)
    {
        connected = false;
        authenticated = false;
        securityFailed = false;

        Serial.println();
        Serial.println("[BLE] CONNECTION LOST");
        Serial.println("[BLE] Connection to PC_server has been lost.");
        Serial.println("[BLE] Restarting scan...");
        Serial.println();

        delay(500);

        BLEDevice::getScan()->start(
            0,
            false
        );
    }
};


// ============================================================
// CONNECT TO SERVER
// ============================================================

bool connectToServer()
{
    if (myDevice == nullptr)
        return false;

    Serial.println();

    Serial.println(
        "[BLE] Connecting to PC_server..."
    );

    if (pClient != nullptr)
    {
        delete pClient;

        pClient = nullptr;
    }

    pClient =
        BLEDevice::createClient();

    pClient->setClientCallbacks(
        new MyClientCallback()
    );


    // --------------------------------------------------------
    // CONNECT
    // --------------------------------------------------------

    if (
        !pClient->connect(
            myDevice
        )
    )
    {
        Serial.println(
            "[BLE] Connection failed."
        );

        return false;
    }


    // --------------------------------------------------------
    // SERVICE
    // --------------------------------------------------------

    BLERemoteService* service =
        pClient->getService(
            SERVICE_UUID
        );

    if (service == nullptr)
    {
        Serial.println(
            "[GATT] Service not found."
        );

        pClient->disconnect();

        return false;
    }

    logEvent(
        "RX",
        "GATT",
        "SERVICE_FOUND",
        nullptr,
        0,
        nullptr,
        0,
        0,
        false,
        SERVICE_UUID
    );


    // --------------------------------------------------------
    // CHARACTERISTIC
    // --------------------------------------------------------

    pRemoteCharacteristic =
        service->getCharacteristic(
            CHARACTERISTIC_UUID
        );

    if (
        pRemoteCharacteristic == nullptr
    )
    {
        Serial.println(
            "[GATT] Characteristic not found."
        );

        pClient->disconnect();

        return false;
    }

    logEvent(
        "RX",
        "GATT",
        "CHARACTERISTIC_FOUND",
        nullptr,
        0,
        nullptr,
        0,
        DIAGNOSTIC_HANDLE,
        true,
        CHARACTERISTIC_UUID
    );


    // --------------------------------------------------------
    // SECURITY
    // --------------------------------------------------------

    Serial.println();

    Serial.println(
        "[SMP] Waiting for BLE authentication..."
    );

    unsigned long securityStart =
        millis();

    while (
        connected
        &&
        !authenticated
        &&
        !securityFailed
        &&
        millis() - securityStart < 30000
    )
    {
        delay(100);
    }


    if (!authenticated)
    {
        Serial.println();

        Serial.println(
            "[SMP] Authentication not completed."
        );

        return false;
    }


    // --------------------------------------------------------
    // NOTIFICATIONS
    // --------------------------------------------------------

    if (
        pRemoteCharacteristic->canNotify()
    )
    {
        // Log the intent BEFORE enabling notifications.
        // registerForNotify() returns void in this ESP32 BLE library,
        // so success/failure is not tested through a return value.
        logEvent(
            "TX",
            "GATT",
            "NOTIFY_ENABLE",
            nullptr,
            0,
            nullptr,
            0,
            DIAGNOSTIC_HANDLE,
            true,
            "Notification subscription"
        );

        pRemoteCharacteristic->registerForNotify(
            notifyCallback
        );
    }


    // --------------------------------------------------------
    // INITIAL KEEP ALIVE
    //
    // NOT LOGGED.
    // --------------------------------------------------------

    if (
        pRemoteCharacteristic->canWrite()
    )
    {
        uint8_t ping[] = {
            0x00
        };

        pRemoteCharacteristic->writeValue(
            ping,
            sizeof(ping),
            true
        );

        last_ping =
            millis();
    }

    return true;
}


// ============================================================
// SEND APPLICATION COMMAND
// ============================================================

void sendCommand(
    const String& command
)
{
    if (
        !connected
        ||
        !authenticated
        ||
        pRemoteCharacteristic == nullptr
    )
    {
        Serial.println(
            "[APP] BLE link is not authenticated."
        );

        return;
    }


    if (
        !pRemoteCharacteristic->canWrite()
    )
    {
        Serial.println(
            "[GATT] Characteristic is not writable."
        );

        return;
    }


    size_t length =
        command.length();


    if (
        length == 0 ||
        length >= 250
    )
    {
        Serial.println(
            "[APP] Payload too large."
        );

        return;
    }


    // --------------------------------------------------------
    // APPLICATION VALUE
    // --------------------------------------------------------

    uint8_t value[250];

    memcpy(
        value,
        command.c_str(),
        length
    );


    // --------------------------------------------------------
    // BUILD FULL ATT PDU
    //
    // 0x12
    // handle low
    // handle high
    // application payload
    // --------------------------------------------------------

    uint8_t pdu[253];

    size_t pduLength =
        buildWriteRequestPdu(
            pdu,
            DIAGNOSTIC_HANDLE,
            value,
            length
        );


    // --------------------------------------------------------
    // LOG FULL PDU
    // --------------------------------------------------------

    uint8_t l2capPdu[264];

    size_t l2capLength =
        buildL2CAPPdu(
            l2capPdu,
            pdu,
            pduLength
        );

    logEvent(
        "TX",
        "L2CAP",
        "DATA",
        l2capPdu,
        l2capLength,
        value,
        length,
        0,
        false,
        "Length=" + String(pduLength) + ", CID=0x0004"
    );

    logEvent(
        "TX",
        "ATT",
        getAttCommandName(0x12),
        pdu,
        pduLength,
        value,
        length,
        DIAGNOSTIC_HANDLE,
        true,
        "Write request"
    );



    // --------------------------------------------------------
    // ACTUAL WRITE
    // --------------------------------------------------------

    bool success =
        pRemoteCharacteristic->writeValue(
            value,
            length,
            true
        );


    if (success)
    {
        uint8_t responsePdu[1];

        size_t responseLength =
            buildWriteResponsePdu(
                responsePdu
            );

        uint8_t l2capPduResponse[8];

        size_t l2capResponseLength =
            buildL2CAPPdu(
                l2capPduResponse,
                responsePdu,
                responseLength
            );

        logEvent(
            "RX",
            "L2CAP",
            "DATA",
            l2capPduResponse,
            l2capResponseLength,
            nullptr,
            0,
            0,
            false,
            "Length=" + String(responseLength) + ", CID=0x0004"
        );

        logEvent(
            "RX",
            "ATT",
            getAttCommandName(0x13),
            responsePdu,
            responseLength,
            nullptr,
            0,
            DIAGNOSTIC_HANDLE,
            true,
            "Write response"
        );
    }
    else
    {
        Serial.println(
            "[GATT] Write failed."
        );
    }


    last_ping =
        millis();

    // Visual separator between application transactions.
    Serial.println();
}


// ============================================================
// SETUP
// ============================================================

void setup()
{
    printResetReason();

    Serial.begin(
        115200
    );

    delay(1000);


    // --------------------------------------------------------
    // GPIO
    // --------------------------------------------------------

    pinMode(
        BOOT_BUTTON_PIN,
        INPUT_PULLUP
    );

    pinMode(
        WRITE_BUTTON_PIN,
        INPUT_PULLUP
    );

    pinMode(
        LED_PIN,
        OUTPUT
    );

    digitalWrite(
        LED_PIN,
        LOW
    );


    // --------------------------------------------------------
    // BLE
    // --------------------------------------------------------

    BLEDevice::init(
        DEVICE_NAME
    );


    // ========================================================
    // SECURITY
    // ========================================================

    pSecurity =
        new BLESecurity();


    // --------------------------------------------------------
    // STATIC PASSKEY
    // --------------------------------------------------------

    pSecurity->setPassKey(
        true,
        BLE_PASSKEY
    );


    // --------------------------------------------------------
    // DISPLAY ONLY
    //
    // Keep the configuration that has already authenticated
    // successfully with your Windows setup.
    // --------------------------------------------------------

    pSecurity->setCapability(
        ESP_IO_CAP_OUT
    );


    // --------------------------------------------------------
    // BONDING + MITM + SECURE CONNECTIONS
    // --------------------------------------------------------

    pSecurity->setAuthenticationMode(
        true,
        true,
        true
    );


    // --------------------------------------------------------
    // FORCE AUTHENTICATION
    // --------------------------------------------------------

    pSecurity->setForceAuthentication(
        true
    );


    // --------------------------------------------------------
    // CALLBACKS
    // --------------------------------------------------------

    BLEDevice::setSecurityCallbacks(
        new MySecurityCallbacks()
    );


    // ========================================================
    // STARTUP
    // ========================================================

    Serial.println();

    Serial.println(
        "ESP32 BLE CLIENT"
    );

    Serial.println(
        "------------------------------------------------------------"
    );

    Serial.printf(
        "Device Name      : %s\n",
        DEVICE_NAME
    );

    Serial.printf(
        "Device ID        : 10\n"
    );

    Serial.println(
        "Role             : BLE CLIENT"
    );

    Serial.printf(
        "Service UUID     : %s\n",
        SERVICE_UUID
    );

    Serial.printf(
        "Characteristic   : %s\n",
        CHARACTERISTIC_UUID
    );

    Serial.printf(
        "Diagnostic Handle: 0x%04X\n",
        DIAGNOSTIC_HANDLE
    );

    Serial.println();

    Serial.println(
        "SECURITY"
    );

    Serial.println(
        "------------------------------------------------------------"
    );

    Serial.println(
        "BLE security      : ESP32 BLE stack"
    );

    Serial.println(
        "Pairing           : Passkey pairing"
    );

    Serial.println(
        "Authentication    : BLE SMP"
    );

    Serial.println(
        "MITM protection   : Enabled"
    );

    Serial.println(
        "------------------------------------------------------------"
    );


    printLegend();


    // --------------------------------------------------------
    // SCAN
    // --------------------------------------------------------

    BLEScan* scan =
        BLEDevice::getScan();

    scan->setAdvertisedDeviceCallbacks(
        new MyAdvertisedDeviceCallbacks()
    );

    scan->setActiveScan(
        true
    );

    Serial.println();

    Serial.println(
        "[GAP] Scanning for PC_server..."
    );

    scan->start(
        0,
        false
    );
}


// ============================================================
// LOOP
// ============================================================

void loop()
{
    // --------------------------------------------------------
    // CONNECT
    // --------------------------------------------------------

    if (doConnect)
    {
        doConnect = false;

        if (!connectToServer())
        {
            Serial.println(
                "[BLE] Connection attempt failed."
            );

            if (pClient != nullptr)
            {
                pClient->disconnect();
            }

            delay(1000);

            Serial.println(
                "[GAP] Scanning for PC_server..."
            );

            BLEDevice::getScan()->start(
                0,
                false
            );
        }
    }

    // Send SECURITY_ESTABLISHED only after the SMP callback has returned.


    // --------------------------------------------------------
    // CONNECTED + AUTHENTICATED
    // --------------------------------------------------------

    if (
        connected
        &&
        authenticated
    )
    {
        // ====================================================
        // BACKGROUND KEEP-ALIVE
        //
        // No logger output.
        // ====================================================

        if (
            millis() - last_ping > 2000
        )
        {
            if (
                pRemoteCharacteristic != nullptr
                &&
                pRemoteCharacteristic->canWrite()
            )
            {
                uint8_t ping[] = {
                    0x00
                };

                pRemoteCharacteristic->writeValue(
                    ping,
                    sizeof(ping),
                    true
                );

                last_ping =
                    millis();
            }
        }


        // ====================================================
        // BUTTON 1 — READ
        // ====================================================

        if (
            digitalRead(
                BOOT_BUTTON_PIN
            ) == LOW
        )
        {
            if (
                pRemoteCharacteristic != nullptr
                &&
                pRemoteCharacteristic->canRead()
            )
            {
                // --------------------------------------------
                // FULL READ REQUEST PDU
                // --------------------------------------------

                uint8_t requestPdu[3];

                size_t requestLength =
                    buildReadRequestPdu(
                        requestPdu,
                        DIAGNOSTIC_HANDLE
                    );

                uint8_t l2capRequestPdu[264];

                size_t l2capRequestLength =
                    buildL2CAPPdu(
                        l2capRequestPdu,
                        requestPdu,
                        requestLength
                    );

                logEvent(
                    "TX",
                    "L2CAP",
                    "DATA",
                    l2capRequestPdu,
                    l2capRequestLength,
                    nullptr,
                    0,
                    0,
                    false,
                    "CID 0x0004"
                );

                logEvent(
                    "TX",
                    "ATT",
                    getAttCommandName(0x0A),
                    requestPdu,
                    requestLength,
                    nullptr,
                    0,
                    DIAGNOSTIC_HANDLE,
                    true,
                    "Read request"
                );


                // --------------------------------------------
                // REAL READ
                // --------------------------------------------

                String value =
                    pRemoteCharacteristic
                    ->readValue()
                    .c_str();


                // --------------------------------------------
                // RESPONSE VALUE
                // --------------------------------------------

                const uint8_t* valueData =
                    reinterpret_cast<
                        const uint8_t*
                    >(
                        value.c_str()
                    );


                // --------------------------------------------
                // FULL RESPONSE PDU
                // --------------------------------------------

                uint8_t responsePdu[260];

                size_t responseLength =
                    buildReadResponsePdu(
                        responsePdu,
                        valueData,
                        value.length()
                    );


                uint8_t l2capResponsePdu[264];

                size_t l2capResponseLength =
                    buildL2CAPPdu(
                        l2capResponsePdu,
                        responsePdu,
                        responseLength
                    );

                logEvent(
                    "RX",
                    "L2CAP",
                    "DATA",
                    l2capResponsePdu,
                    l2capResponseLength,
                    valueData,
                    value.length(),
                    0,
                    false,
                    "Length=" + String(responseLength) + ", CID=0x0004",
                    true
                );

                logEvent(
                    "RX",
                    "ATT",
                    getAttCommandName(0x0B),
                    responsePdu,
                    responseLength,
                    valueData,
                    value.length(),
                    DIAGNOSTIC_HANDLE,
                    true,
                    "Read response",
                    true
                );


            }


            delay(1000);
        }


        // ====================================================
        // BUTTON 2 — TOGGLE LOCK
        // ====================================================

        if (
            digitalRead(
                WRITE_BUTTON_PIN
            ) == LOW
        )
        {
            sendCommand(
                "TOGGLE_LOCK"
            );

            delay(1000);
        }
    }


    delay(50);
}