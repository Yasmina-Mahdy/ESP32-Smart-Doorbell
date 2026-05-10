# ESP32 Security System

A local security system built on an ESP32-S3 that detects faces via a camera, detects motion via a PIR sensor, and streams events to a live web dashboard in real time.

```
ESP32-S3 (Camera + PIR) -> MQTT / HTTP POST -> Mosquitto + Flask -> Dashboard
```

## What is there

- **PIR motion detection** - ESP32 wakes on motion, grabs a timestamp from NTP, publishes over MQTT
- **Face detection** - ESP32-S3 camera runs on-device AI face detection; on detection, captures a JPEG and POSTs it to the server
- **Live dashboard** - Flask server subscribes to MQTT, stores events in SQLite, serves a real-time dashboard via SocketIO
- **Image viewer** - captured face images are saved and viewable from the dashboard
- **Audio upload** - endpoint ready for audio recording upload (coming soon)

## Stack

- **ESP32-S3** - face detection firmware written in C++ using ESP-IDF v5.4 and esp-who
- **ESP32** - PIR/MQTT firmware written in C using ESP-IDF
- **Mosquitto** - MQTT broker running in Docker
- **Python/Flask + SocketIO** - subscribes to MQTT, stores events, serves real-time dashboard
- **SQLite** - lightweight local database, no setup needed
- **Docker Compose** - runs Mosquitto and Flask together with one command

## Project structure

```
project/
|-- docker-compose.yml
|-- pir_mqtt/                          # ESP32 PIR firmware
|   |-- main/main.c
|-- esp-who/                           # ESP32-S3 face detection firmware
|   |-- examples/human_face_detection/
|       |-- terminal/
|           |-- main/app_main.cpp
|-- mosquitto/
|   |-- config/
|       |-- mosquitto.conf
|       |-- pwfile
|-- webserver/                         # Flask server and dashboard
    |-- app.py
    |-- database.py
    |-- mqtt_client.py
    |-- Dockerfile
    |-- requirements.txt
    |-- static/style.css
    |-- templates/dashboard.html
```

## Running locally

**Requirements:** Docker Desktop

```bash
docker compose up --build
```

Open the dashboard at `http://localhost:5000`, or from any device on the same network at `http://<your-pc-ip>:5000`.

## ESP32 PIR setup

Open `pir_mqtt/main/main.c` and update:

```c
#define WIFI_SSID      "your-network"
#define WIFI_PASS      "your-password"
#define MQTT_BROKER    "your-pc-local-ip"
```

Flash using ESP-IDF:

```bash
idf.py build flash monitor
```

## ESP32-S3 Face Detection setup

Open `esp-who/examples/human_face_detection/terminal/main/app_main.cpp` and update:

```c
#define WIFI_SSID   "your-network"
#define WIFI_PASS   "your-password"
```

And update the server IP:

```c
snprintf(url, sizeof(url), "http://<your-pc-local-ip>:5000/upload/image");
```

Flash using ESP-IDF (from the terminal directory):

```bash
cd esp-who/examples/human_face_detection/terminal
idf.py set-target esp32s3
idf.py build flash monitor
```

When a face is detected, the ESP32-S3 captures a JPEG and POSTs it to the server automatically. Images are saved under `uploads/images/` and logged in the dashboard.

## How it works

```
PIR triggered  ->  MQTT publish      ->  Flask logs motion event  ->  Dashboard updates live
Face detected  ->  HTTP POST JPEG    ->  Flask saves image + logs event  ->  Dashboard updates live
```

## Deploying to a VPS

Since everything is Dockerized, moving to a VPS is straightforward:

1. Install Docker on the VPS
2. Clone the repo
3. Run `docker compose up --build`
4. Update both ESP32 firmwares with the VPS public IP and reflash

The dashboard becomes publicly accessible at `http://<vps-ip>:5000`.

## Planned features

- Audio recording on button press (audio upload via HTTP POST)
- Audio playback in dashboard
