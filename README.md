# ESP32 Security System

A local security system built on an ESP32 that detects motion via a PIR sensor and streams events to a live web dashboard. [updated as we go]

## What is there [updated as we go]

The ESP32 wakes on PIR motion, grabs a timestamp from NTP, and publishes a message over MQTT. A Python server subscribes to those messages, logs them to a SQLite database, and serves a dashboard you can open from any device on the same network.

```
ESP32 (PIR) → MQTT → Mosquitto broker → Python/Flask → Dashboard
```

## Stack

- **ESP32** — firmware written in C using ESP-IDF v6.0
- **Mosquitto** — MQTT broker running in Docker
- **Python/Flask** — subscribes to MQTT, stores events, serves dashboard
- **SQLite** — lightweight local database, no setup needed
- **Docker Compose** — runs Mosquitto and Flask together with one command

## Project structure

```
project/
├── docker-compose.yml
├── esp_pir/              # ESP32 firmware
│   └── main/main.c
└── mosquitto/
    └── config/
            ├── mosquitto.conf
            └── pwfile
└── webserver/            # Flask server and dashboard
    ├── app.py
    ├── database.py
    ├── mqtt_client.py
    ├── Dockerfile
    ├── static/style.css
    └── templates/dashboard.html
```

## Running locally

**Requirements:** Docker Desktop

```bash
docker compose up --build
```

Open the dashboard at `http://localhost:5000`, or from any device on the same network at `http://<your-pc-ip>:5000`.

## ESP32 setup

Open `esp_pir/main/main.c` and update:

```c
#define WIFI_SSID      "your-network"
#define WIFI_PASS      "your-password"
#define MQTT_BROKER    "your-pc-local-ip"
```

Flash using ESP-IDF:

```bash
idf.py build flash monitor
```

## Deploying to a VPS

Since everything is Dockerized, moving to a VPS is straightforward:

1. Install Docker on the VPS
2. Clone the repo
3. Run `docker compose up --build`
4. Update the ESP32 firmware with the VPS public IP and reflash

The dashboard becomes publicly accessible at `http://<vps-ip>:5000`.

## Planned features

- Face detection on motion trigger (image capture via HTTP POST)
- Audio recording on button press (audio upload via HTTP POST)
- Image and audio viewer in dashboard
