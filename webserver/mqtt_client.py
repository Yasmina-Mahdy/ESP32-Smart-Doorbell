from datetime import datetime
import paho.mqtt.client as mqtt
import time
from database import insert_event

MQTT_BROKER = "172.20.10.2"  # PC's local IP, changes per network (see dependencies.txt)
MQTT_PORT = 1883
MQTT_TOPIC = "home/pir"
MQTT_USER = "esp32"
MQTT_PASS = "esp32"


def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("Connected to MQTT broker")
        client.subscribe(MQTT_TOPIC)
    else:
        print(f"MQTT connection failed with code {rc}")


def on_message(client, userdata, msg):
    timestamp = datetime.now().strftime("%I:%M:%S %p %d/%m/%Y")
    print(f"Received: {msg.payload.decode()}")
    insert_event("pir", timestamp, "Motion Detected")


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