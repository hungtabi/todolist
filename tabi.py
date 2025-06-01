import fcntl
import struct
import os
import time
import paho.mqtt.client as mqtt
import ssl
import json
import _thread

MY_IOCTL_MAGIC = ord('a')

def _IOR(magic_num, command_num, size):
    IOC_READ = 2
    return (IOC_READ << 30) | (magic_num << 8) | (command_num << 0) | (size << 16)

def _IOW(magic_num, command_num, size):
    IOC_WRITE = 1
    return (IOC_WRITE << 30) | (magic_num << 8) | (command_num << 0) | (size << 16)

WR_DOOR_STA = _IOW(MY_IOCTL_MAGIC, ord('a'), struct.calcsize('i'))  # ghi int
RD_DOOR_STA = _IOR(MY_IOCTL_MAGIC, ord('b'), struct.calcsize('i'))  # đọc int

WR_C_PASS = _IOW(MY_IOCTL_MAGIC, ord('d'), 6)  # ghi char[6]
RD_C_PASS = _IOR(MY_IOCTL_MAGIC, ord('e'), 6)  # đọc char[6]

DEVICE_PATH = "/dev/tabi_device"
def write_door_status(status: int):
    fd = os.open(DEVICE_PATH, os.O_RDWR)
    data = struct.pack('i', status)
    fcntl.ioctl(fd, WR_DOOR_STA, data)
    os.close(fd)

def read_door_status():
    fd = os.open(DEVICE_PATH, os.O_RDWR)
    buf = bytearray(4)
    fcntl.ioctl(fd, RD_DOOR_STA, buf)
    status = struct.unpack('i', buf)[0]
    os.close(fd)
    return status

def write_password(password: str):
    if len(password) != 6:
        raise ValueError("Password has 6 character")
    fd = os.open(DEVICE_PATH, os.O_RDWR)
    fcntl.ioctl(fd, WR_C_PASS, password.encode())
    os.close(fd)
    print(f"New password update: {password}")

def read_password():
    fd = os.open(DEVICE_PATH, os.O_RDWR)
    buf = bytearray(6)
    fcntl.ioctl(fd, RD_C_PASS, buf)
    password = buf.decode()
    os.close(fd)
    return password


def on_connect(client, userdata, flags, rc):
    print("Connected to AWS IoT: " + str(rc))
    client.subscribe("raspi/control")

def on_message(client, userdata, msg):
    data = json.loads(msg.payload.decode())
    if "password" in data:
        if data["password"] == read_password():
            print("Door open")
            write_door_status(1)
        else:
            print("Door close")
            write_door_status(0)
    elif "change" in data:
        write_password(data["change"])
        
    
client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message
client.tls_set(ca_certs='./RootCA.pem', certfile='./certificate.pem.crt', keyfile='./private.pem.key', tls_version=ssl.PROTOCOL_SSLv23)
client.tls_insecure_set(True)
client.connect("a2cuiohotiqavz-ats.iot.ap-southeast-2.amazonaws.com", 8883, 60)
client.loop_start()


if __name__ == "__main__":
    while True:
        door_state = read_door_status()
        password = read_password()
        msg = {
            "door state": door_state,
            "password": password
        }
        client.publish("raspi/data", payload=json.dumps(msg), qos=0)
        time.sleep(2)
