from datetime import datetime
import paho.mqtt.client as mqtt
import time
from database import insert_event

MQTT_BROKER = "mosquitto"
MQTT_PORT = 1883
MQTT_TOPIC = "home/pir"
MQTT_USER = "esp32"
MQTT_PASS = "esp32"

_socketio = None

def set_socketio(sio):
    global _socketio
    _socketio = sio


def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("Connected to MQTT broker")
        client.subscribe(MQTT_TOPIC)
    else:
        print(f"MQTT connection failed with code {rc}")


def on_message(client, userdata, msg):
    timestamp = datetime.now().strftime("%I:%M:%S %p %d/%m/%Y")
    print(f"Received: {msg.payload.decode()}")
    row_id = insert_event("pir", timestamp, "Motion Detected")
    if _socketio:
        _socketio.emit("new_event", {
            "id": row_id,
            "event_type": "pir",
            "timestamp": timestamp,
            "data": "Motion Detected",
            "file_path": None
        })


def start_mqtt():
    client = mqtt.Client()
    client.username_pw_set(MQTT_USER, MQTT_PASS)
    client.on_connect = on_connect
    client.on_message = on_message

    while True:
        try:
            client.connect(MQTT_BROKER, MQTT_PORT, 60)
            client.loop_forever()
            break
        except Exception as e:
            print(f"MQTT connection failed: {e}, retrying in 5 seconds...")
            time.sleep(5)