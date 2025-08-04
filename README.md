# Autonomous Maze-Navigating Robot 🤖

## 🎥 Project Demo

Watch the video demonstration of our autonomous maze-navigating robot in action:

[![Watch the demo](https://img.youtube.com/vi/NGwKag2HGF8/0.jpg)](https://www.youtube.com/watch?v=NGwKag2HGF8)

---

## Project by:
**Tali Grayzel** ([taligrayzel](https://github.com/taligrayzel))  

**Nitay Nahary** ([NitayNahary](https://github.com/NitayNahary))

**Shachar Asher Cohen** ([cohenshachar](https://github.com/cohenshachar))

---

## 📘 Project Overview

This project features an **autonomous robot** that navigates a user-built maze using onboard sensors and motors. The robot:

- Uses **3 Time-of-Flight (ToF) distance sensors** to perceive its environment
- Drives using **2 DC motors with encoders** for precise movement and positioning
- Tracks and logs data in **real time** to a **Firebase database**
- Includes a **web-based user interface** that displays sensor data, motor states, and maze traversal in real time
- Five real-time tasks controlling the system

Built as part of the **ICST Smart Technologies Program** at the **Technion - Taub Faculty of Computer Science**.

> 🧠 The robot is fully autonomous once placed in the maze and continuously sends telemetry data to the cloud while navigating.

---

## 📁 Folder Description

| Folder        | Description                                                                 |
|---------------|-----------------------------------------------------------------------------|
| `Robot/Main/` | Source code for the ESP32 (firmware)                                        |
| `Documentation/` | Wiring diagrams and task timing                                           |

---
## 🔧 SDK and Toolchain Versions

- **ESP32 Arduino Core**: `esp32 by Espressif Systems` - version **2.0.17**
- **Arduino AVR Boards**: `Arduino` - version **1.8.6**
- **IDE Used**: Arduino IDE (or compatible with PlatformIO)

---

## 📚 Arduino / ESP32 Libraries Used

- `Firebase Arduino Client Library for ESP8266 and ESP32 by Mobizt` - version **4.4.17**  
- `Adafruit VL53L0X` - version **1.2.4**  

---

## 🔌 Connection Diagram

- See `/Documentation/wiring.jpg` for full wiring layout
- Power: 5v battery → ESP32  
- Motors: Controlled via motor driver connected to ESP32 PWM pins  
- Sensors: I²C ToF sensors mounted front-left, front-center, and front-right
- See `/Documentation/tasks.png` for  task schedualing timing
---

## 🎓 Acknowledgements

This project was developed as part of **ICST - The Interdisciplinary Center for Smart Technologies**  
📍 Taub Faculty of Computer Science, Technion – Israel Institute of Technology  
🔗 https://icst.cs.technion.ac.il/


