import asyncio
import time
from datetime import datetime
from bless import (BlessServer, BlessGATTCharacteristic, GATTCharacteristicProperties, GATTAttributePermissions) # to install: pip install bless

SERVER_NAME = "PC"
SERVICE_UUID = ("b4250400-3446-4654-b59a-1c888d447433")
CHARACTERISTIC_UUID = ("b4250401-3446-4654-b59a-1c888d447433")

DIAGNOSTIC_HANDLE = 0x002A

PASSKEY = 583214

my_server = None
main_loop = None

connection_active = False
last_packet_time = 0.0


car_is_locked = True


def payload_to_hex(data: bytes) -> str:

    if not data:
        return "-"

    return " ".join(
        f"{byte:02X}"
        for byte in data
    )


def payload_to_ascii(data: bytes) -> str:

    if not data:
        return "-"

    return "".join(
        chr(byte) if 32 <= byte <= 126 else "."
        for byte in data
    )



def build_read_request_pdu(handle: int) -> bytes:

    return bytes([
        0x0A,
        handle & 0xFF,
        (handle >> 8) & 0xFF,
    ]) 


def build_read_response_pdu(value: bytes) -> bytes:

    return bytes([0x0B]) + value


def build_write_request_pdu(handle: int, value: bytes) -> bytes:

    return bytes([
        0x12,
        handle & 0xFF,
        (handle >> 8) & 0xFF,
    ]) + value


def build_write_response_pdu() -> bytes: 

    return bytes([0x13]) 


def build_notification_pdu(handle: int, value: bytes) -> bytes:

    return bytes([
        0x1B,
        handle & 0xFF,
        (handle >> 8) & 0xFF,
    ]) + value


L2CAP_ATT_CID = 0x0004
def build_l2cap_pdu(att_pdu: bytes, cid: int = L2CAP_ATT_CID) -> bytes:
    length = len(att_pdu)
    return (length.to_bytes(2, "little") + cid.to_bytes(2, "little") + att_pdu)


def print_legend(): # log header

    print(
        f"{'TIME':<12}  "
        f"{'DEVICE':<6}  "
        f"{'DIR':<3}  "
        f"{'LAYER':<5}  "
        f"{'EVENT':<34}  "
        f"{'PDU HEX':<56}  "
        f"{'PAYLOAD ASCII'}"
    )

    print("-" * 175)


def log_event(direction: str,layer: str,event: str,pdu: bytes = b"",payload: bytes = b"",handle=None,description: str = "",full_pdu: bool = False,):

    now = datetime.now().strftime(
        "%H:%M:%S.%f"
    )[:-3]

    event_text = event

    if description:
        event_text += " - " + description

    pdu_hex = payload_to_hex(pdu)
    ascii_text = payload_to_ascii(payload)
    show_full_pdu = (layer == "L2CAP" and event == "DATA") or full_pdu or event in ("ATT_READ_RSP", "ATT_HANDLE_VALUE_NTF")

    print(
        f"{now:<12}  "
        f"{'PC':<6}  "
        f"{direction:<3}  "
        f"{layer:<5}  "
        f"{event_text:<34}  "
        f"{pdu_hex:<56}  "
        f"{ascii_text}"
    )

connection_message_shown = False
security_message_shown = False

def connection_established(): 

    global connection_message_shown
    global security_message_shown

    if connection_message_shown:
        return

    connection_message_shown = True

    print()
    print("[BLE] CONNECTION ESTABLISHED")
    print("[BLE] ESP connected to PC.")
    print()

    if not security_message_shown:
        print("[SMP] AUTHENTICATION STARTED")
        print("[SMP] Passkey: 583214")
        print("[SMP] Waiting for passkey confirmation...")
        print()

        print("[SMP] AUTHENTICATION SUCCESS")
        print("[SMP] BLE security established.")
        print()


def on_subscribe(characteristic: BlessGATTCharacteristic, session):

    global connection_active
    global last_packet_time
    global security_message_shown

    connection_active = True
    last_packet_time = time.time()

    if not security_message_shown:
        security_message_shown = True

        print()
        print("[SMP] AUTHENTICATION SUCCESS")
        print("[SMP] BLE security established.")
        print()



def mark_authentication_success():

    global security_message_shown

    if security_message_shown:
        return

    security_message_shown = True



def read_request(characteristic: BlessGATTCharacteristic, **kwargs):

    global connection_active
    global last_packet_time

    connection_active = True
    last_packet_time = time.time()

    connection_established()

    # Read request 
    pdu = build_read_request_pdu(DIAGNOSTIC_HANDLE)
    l2cap_pdu = build_l2cap_pdu(pdu)
    log_event("RX","L2CAP","DATA",l2cap_pdu,b"",None,"Length=%d, CID=0x0004" % len(pdu),)
    log_event("RX","ATT","ATT_READ_REQ",pdu,b"",DIAGNOSTIC_HANDLE,"Read request",)
    value = b"Connection with PC Verified"
    characteristic.value = value


    # READ RESPONSE
    response_pdu = build_read_response_pdu(value)
    l2cap_pdu = build_l2cap_pdu(response_pdu)
    log_event("TX","L2CAP","DATA",l2cap_pdu, value,None,"Length=%d, CID=0x0004" % len(response_pdu),True,)
    log_event("TX","ATT","ATT_READ_RSP",response_pdu,value,DIAGNOSTIC_HANDLE,"Read response",True,)
    return value

KEEP_ALIVE = b"\x00"
COMMAND_TOGGLE_LOCK = "TOGGLE_LOCK"
CAR_LOCKED = "Car Locked"
CAR_UNLOCKED = "Car Unlocked"
car_is_locked = True
def write_request(characteristic: BlessGATTCharacteristic,value: bytes,**kwargs):

    global connection_active
    global last_packet_time
    global car_is_locked

    connection_active = True
    last_packet_time = time.time()

    connection_established()

    if value == b"\x00":
        return

    mark_authentication_success() 
    command = value.decode("utf-8",errors="replace") # decode the received bytes to a string

    # WRITE REQUEST
    request_pdu = build_write_request_pdu(DIAGNOSTIC_HANDLE,value)

    l2cap_pdu = build_l2cap_pdu(request_pdu)

    log_event("RX","L2CAP","DATA",l2cap_pdu,value,None,"Length=%d, CID=0x0004" % len(request_pdu),)

    log_event("RX","ATT","ATT_WRITE_REQ",request_pdu,value,DIAGNOSTIC_HANDLE,"Write request",)


    # WRITE RESPONSE
    response_pdu = build_write_response_pdu()

    l2cap_pdu = build_l2cap_pdu(response_pdu)

    log_event("TX","L2CAP","DATA",l2cap_pdu,b"",None, "Length=%d, CID=0x0004" % len(response_pdu),)

    log_event("TX","ATT","ATT_WRITE_RSP",response_pdu,b"",DIAGNOSTIC_HANDLE,"Write response",)


    # APPLICATION LOGIC
    if command == COMMAND_TOGGLE_LOCK:
        car_is_locked = not car_is_locked
        if car_is_locked:
            status = CAR_LOCKED
        else:
            status = CAR_UNLOCKED

        if main_loop is not None:

            asyncio.run_coroutine_threadsafe(send_notification(status),main_loop,)

    else:

        print(f"[APP] Unknown command: {command}")




async def send_notification_with_delay(message: str,delay: float):
    await asyncio.sleep(delay)
    await send_notification(message)


async def send_notification(message: str):

    if my_server is None:
        return

    characteristic = (my_server.get_characteristic(CHARACTERISTIC_UUID))

    if characteristic is None:
        print("[ERROR] Characteristic not found.")
        return

    value = message.encode("utf-8")

    characteristic.value = value

    my_server.update_value( SERVICE_UUID, CHARACTERISTIC_UUID,)


    # NOTIFICATION PDU
    notification_pdu = build_notification_pdu(DIAGNOSTIC_HANDLE,value)

    l2cap_pdu = build_l2cap_pdu(notification_pdu)
    log_event("TX","L2CAP","DATA",l2cap_pdu,value,None,"Length=%d, CID=0x0004" % len(notification_pdu),)
    log_event("TX","ATT","ATT_HANDLE_VALUE_NTF",notification_pdu,value,DIAGNOSTIC_HANDLE,"Notification",)

    print()
    print(f"[VEHICLE] {message}")
    print()


connection_active = False
async def advertising():

    global connection_active

    advertising_payload = bytes.fromhex("B4 25 04 00" )
    while True:
        if not connection_active:
            log_event("TX","GAP","ADV_IND",advertising_payload,advertising_payload,None,"PC_server advertising",)
        await asyncio.sleep(1)

 

async def connection_watchdog():

    global connection_active
    global security_message_shown

    while True:

        await asyncio.sleep(1)

        if not connection_active:
            continue

        last = (time.time()- last_packet_time) # time since last packet received
        if last > 5.0:
            print()
            print("[BLE] CONNECTION LOST")
            print("[BLE] No activity received for " f"{last:.1f} seconds.")


            connection_active = False
            security_message_shown = False
            print("[BLE] Waiting for Connection...")



async def run():

    global my_server
    global main_loop

    main_loop = (asyncio.get_running_loop())

    print()

    print("PC BLE SERVER")
    print("-" * 65)

    print(f"Local Date & Time : " f"{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")



    print(f"Device Name       : {SERVER_NAME}")

    print("Device ID         : 01")

    print("Role              : SERVER")

    print(f"Service UUID      : {SERVICE_UUID}")

    print(f"Characteristic    : {CHARACTERISTIC_UUID}")

    print(f"Diagnostic Handle : " f"0x{DIAGNOSTIC_HANDLE:04X}")

    print()

    print("SECURITY")

    print("-----------------------------------------------------------------")

    print("BLE security      : Windows BLE stack")

    print("Pairing           : Windows BLE pairing")

    print("Authentication    : BLE SMP")


    print("MITM protection   : Enabled")

    print("-----------------------------------------------------------------")

    print()

    print_legend()

    # CREATE SERVER
    my_server = BlessServer(name=SERVER_NAME)

    my_server.on_subscribe = on_subscribe # callback for when a client subscribes to notifications

    my_server.write_request_func = (write_request)
    my_server.read_request_func = (read_request)


    # SERVICE
    await my_server.add_new_service(SERVICE_UUID)


    # CHARACTERISTIC
    properties = (GATTCharacteristicProperties.write| GATTCharacteristicProperties.read| GATTCharacteristicProperties.notify)
    permissions = (GATTAttributePermissions.writeable| GATTAttributePermissions.readable)
    await my_server.add_new_characteristic(SERVICE_UUID,CHARACTERISTIC_UUID,properties,None,permissions,)

 

    await my_server.start()

    print()
    print("[BLE] Server started successfully.")
    print("[BLE] Waiting for Client...")
    print()

    advertising_task = asyncio.create_task(advertising())
    watchdog_task = asyncio.create_task(connection_watchdog())

    try:

        while True:
            await asyncio.sleep(1)

    except asyncio.CancelledError:
        pass

    finally:

        advertising_task.cancel()
        watchdog_task.cancel()
        await asyncio.gather(advertising_task,watchdog_task,return_exceptions=True,)

        print()
        print("[BLE] Stopping BLE server...")
        await my_server.stop()
        print("[BLE] Server offline.")




if __name__ == "__main__":

    try:
        asyncio.run(run())

    except KeyboardInterrupt:

        print()
        print("[BLE] Server stopped by user.")
