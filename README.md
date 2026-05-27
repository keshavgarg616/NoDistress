# NoDistress – Autonomous Distress Monitoring Rover

> AI-powered rover for autonomous physical and emotional distress monitoring.

![Poster](https://github.com/keshavgarg616/NoDistress/blob/main/PresentationPoster.png)

---

# Overview

Healthcare and safety monitoring systems today are often reactive instead of proactive. Wearable panic buttons can be forgotten or impossible to activate during emergencies, while static cameras suffer from blind spots and cannot actively respond to a situation.

NoDistress was built to explore a smarter alternative: an autonomous AI-powered rover capable of physically navigating toward individuals, analyzing their posture and emotional state in real time, and identifying potential distress situations without requiring any wearable device.

Using computer vision, autonomous tracking, and real-time AI analysis, the rover continuously monitors for signs such as falls, dangerous body positions, fear, or emotional distress. By combining mobility with intelligent perception, NoDistress aims to create a more adaptive and human-centered approach to proactive safety monitoring in environments like hospitals, assisted living facilities, and smart homes.

NoDistress is a smart rover that autonomously tracks individuals and analyzes:

- Body posture
- Facial expressions
- Distress indicators

The system computes a real-time **Distress Score** and streams results to a live dashboard for rapid intervention.

---

# Features

- Autonomous human tracking
- Real-time posture detection
- Facial emotion analysis
- Live distress dashboard
- Automatic rover navigation
- Investigation mode for closer facial inspection
- Fault-tolerant AI pipeline

---

# Tech Stack

## Hardware

- ESP32-CAM
- L298N Motor Driver
- 4 DC Motors
- Rover Chassis
- Battery Pack

## Software

- Python
- Flask
- OpenCV
- MediaPipe
- DeepFace

---

# Project Structure

```bash
NoDistress/
│
├── CV/
│   ├── ExpressionDetector/
│   │   └── emotion_detection.py
│   │
│   └── PostureDetector/
│       └── pose_detection.py
│
├── ESP32-CAM Code/
│   ├── src/
│   │   └── main.cpp
│   ├── platformio.ini
│   └── .gitignore
│
├── app.py
├── requirements.txt
├── README.md
└── PresentationPoster.png
```

---

# Configuration

Before running the project, update the following parameters.

## 1. WiFi Credentials (ESP32)

Inside:

```cpp
ESP32-CAM Code/src/main.cpp
```

Update:

```cpp
const char *ssid = "YOUR_WIFI_NAME";
const char *password = "YOUR_WIFI_PASSWORD";
```

---

## 2. ESP32 IP Address (Python Backend)

Inside:

```python
app.py
```

Update:

```python
ESP32_IP = "YOUR_ESP32_IP"
```

Example:

```python
ESP32_IP = "192.168.1.25"
```

You can find the IP from the ESP32 Serial Monitor after boot.

---

# How to Run

## 1. Install Python Dependencies

```bash
pip install -r requirements.txt
```

---

## 2. Flash ESP32-CAM

Using PlatformIO:

```bash
cd "ESP32-CAM Code"
pio run --target upload
```

Open Serial Monitor:

```bash
pio device monitor
```

Copy the printed ESP32 IP address.

---

## 3. Update IP in app.py

Set:

```python
ESP32_IP = "YOUR_ESP32_IP"
```

---

## 4. Run Backend Server

```bash
python app.py
```

Server starts at:

```bash
http://localhost:3000
```

---

# Web Endpoints

| Endpoint       | Purpose              |
| -------------- | -------------------- |
| `/capture`     | Single camera frame  |
| `/stream`      | Live MJPEG stream    |
| `/action?go=F` | Rover movement       |
| `/latest`      | Latest AI analysis   |
| `/status`      | Backend health check |

---

# Rover Commands

| Command | Action   |
| ------- | -------- |
| F       | Forward  |
| B       | Backward |
| L       | Left     |
| R       | Right    |
| S       | Stop     |

---

# Wiring Diagram

## ESP32-CAM ↔ L298N Connections

| ESP32-CAM Pin | L298N Pin |
| ------------- | --------- |
| GPIO14        | IN1       |
| GPIO15        | IN2       |
| GPIO13        | IN3       |
| GPIO12        | IN4       |
| GND           | GND       |

---

## Motor Connections

| L298N Output | Motors            |
| ------------ | ----------------- |
| OUT1 + OUT2  | Left-side motors  |
| OUT3 + OUT4  | Right-side motors |

Use motors on each side in parallel.

---

## Power Connections

| Component | Power         |
| --------- | ------------- |
| ESP32-CAM | 5V            |
| L298N     | Battery Pack  |
| Motors    | Through L298N |

Make sure all grounds are connected together.

---

# Applications

- Elderly care
- Hospitals
- Assisted living
- Industrial safety
- Smart surveillance

---

---

Made with ❤️ by

- Keshav Garg
- Shashank Shridhar
- Thien Bui
- Mervyn Panicker
