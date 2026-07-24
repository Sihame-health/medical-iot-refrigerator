# IoT-Based Medical Refrigerator

## 📌 Project Overview
This repository contains an academic IoT project developed as part of a Master's program in Digital Engineering for Healthcare.
The project demonstrates a connected medical refrigerator prototype using embedded systems and IoT technologies for monitoring, control, and supervision — addressing a real problem in healthcare: maintaining the cold chain for sensitive medical products (blood, vaccines, reagents, platelets).

## 🎯 Key Features
- Adaptive temperature regulation with hysteresis control, tuned per medical storage mode
- Peltier-based cooling system, driven by a MOSFET + transistor gate-drive stage
- Door status monitoring with a tiered open-door alert (pre-alert → critical alarm + auto-close)
- Content presence detection (IR sensor) — cooling shuts off automatically when the fridge is empty, to save energy and protect the Peltier module
- MQTT-based communication between the ESP32 and the supervision layer
- Real-time dashboard using Node-RED (temperature gauge, history chart, live status, remote door control)
- Time-series data storage in InfluxDB for traceability
- Indoor positioning via Wi-Fi RSSI fingerprinting

## 🌡️ Storage Modes

| Mode | Product | Target Temp | Hysteresis |
|---|---|---|---|
| SANG | Blood | +4°C | ±2°C |
| VACCINS | Vaccines | +5°C | ±3°C |
| REACTIFS | Lab reagents | +10°C | ±2°C |
| PLAQUETTES | Platelets | +23°C | ±2°C |

Cooling logic: if the fridge is empty, the Peltier stays OFF regardless of mode. If it's full, the Peltier turns ON above `target + hysteresis` and OFF below `target − hysteresis`, holding steady in between to avoid rapid cycling.

## 🛠️ Technologies Used
- **Microcontroller:** ESP32 (Embedded C/C++, developed in VS Code)
- **Sensors & Actuators:** DHT11 (temperature), reed switch (door), IR obstacle sensor (content), SG90 servo (door), Peltier TEC1-12706 (cooling)
- **Communication:** MQTT (Eclipse Mosquitto broker)
- **Supervision:** Node-RED (dashboard + control logic)
- **Storage:** InfluxDB (time-series data)

## 📡 MQTT Topics

**Published by ESP32:** `frigo/temp/current`, `frigo/door/status`, `frigo/obstacle`, `frigo/peltier/state`, `frigo/location`
**Subscribed by ESP32:** `frigo/mode/set`, `frigo/servo/cmd`

## 📁 Repository Structure
- `/firmware` → ESP32 source code
- `/node-red` → Node-RED flows
- `/images` → Prototype and system images
- `/docs` → Architecture, wiring diagrams, and full technical report

## 🎥 Project Demonstration
👉 Project demo video:

## ⚠️ Disclaimer
This project was developed for academic and educational purposes only and is not intended for clinical or medical use.

## 👩‍🎓 Author
Siham Ait taleb
Master's student in Digital Engineering for Healthcare
