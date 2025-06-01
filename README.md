# door-sercurity
# Flutter app
![image alt](https://github.com/hungtabi/door-sercurity/blob/main/Screenshot%202025-06-01%20at%2013.37.16.png)
# Matrix 4x4 keypad
![image alt](https://github.com/hungtabi/door-sercurity/blob/5e26b874e96a3dd0ff748ea6a37e0cad62143190/27899.png)
# A: for clear 1 character of the password
# B: to clear all character of the password
# C: go to change the password mode and otherwise

This project implements a smart door control system using a 4x4 matrix keypad, a push button, and a servo motor to open or close the door. The system also connects to AWS IoT Core to:
* Publish door status to the cloud.
* Remotely control the door (open/close or change password) via MQTT.
The project is developed for Raspberry Pi running Embedded Linux.

🧰 Features
* 🔢 Enter 6-digit password via 4x4 matrix keypad to open the door.
* 🔘 Physical push button to toggle door open/close.
* 🔒 Password stored in kernel driver using ioctl.
* 📡 Publish door_state and current password to AWS Cloud via MQTT.
* ☁️ Receive commands from AWS to:
    * Open/close the door remotely.
    * Change the password.

🛠️ Hardware Requirements
* Raspberry Pi (any model with GPIO support)
* 4x4 Matrix Keypad
* Push Button
* Servo Motor
* AWS IoT Account (with certificates)

📦 Software Stack
* Kernel Module Driver for:
    * Servo control
    * Password management (via ioctl)
* Python Script to:
    * Handle AWS IoT MQTT communication
    * Interface with kernel driver
* MQTT Protocol for cloud communication
* AWS IoT Core for secure message handling

📡 AWS Topics
* Publish Topic: raspi/data
* Subscribe Topic: raspi/control
Example Payloads:
* 🔑 Change Password:
{ "change": "654321" }
* 🚪 Open Door (if password matches):
{ "password": "123456" }

🚀 How to Run
1. Compile and insert kernel module:
bash
make
sudo insmod test.ko
2. Run Python MQTT client:
bash
sudo python3 tabi.py
Make sure the following files are in the same directory:
* certificate.pem.crt
* private.pem.key
* AmazonRootCA1.pem (or RootCA.pem)


* Flutter Mobile App Integration
The project also includes a Flutter mobile app that connects to AWS IoT via MQTT (using TLS) to remotely monitor and control the door. This enables users to open/close the door or change the password directly from a smartphone.

🧰 Features in Flutter App
✅ Connect to AWS IoT Core securely (TLS certificates)

🔓 Open/Close door by sending the password

🔑 Change door password remotely

📡 Display real-time door status

📦 Flutter Stack
mqtt_client for MQTT over TLS

flutter_secure_storage for secure local config

Custom UI to interact with door system
