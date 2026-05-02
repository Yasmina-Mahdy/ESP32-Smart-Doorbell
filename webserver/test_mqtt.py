import paho.mqtt.client as mqtt

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("Connected!")
        client.subscribe("home/pir")
    else:
        print(f"Failed, rc={rc}")

def on_message(client, userdata, msg):
    print(f"Got message: {msg.payload.decode()}")

client = mqtt.Client()
client.username_pw_set("esp32", "esp32")
client.on_connect = on_connect
client.on_message = on_message
client.connect("172.20.10.2", 1883, 60)
client.loop_forever()
