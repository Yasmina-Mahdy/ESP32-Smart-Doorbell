import threading
import datetime
import os
from flask import Flask, render_template, request
from database import init_db, get_all_events
from mqtt_client import start_mqtt

app = Flask(__name__)
os.makedirs("static/images", exist_ok=True)


@app.route("/")
def dashboard():
    rows = get_all_events()
    return render_template("dashboard.html", rows=rows)


@app.route("/upload/image", methods=["POST"])
def upload_image():
    ts = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
    path = f"static/images/face_{ts}.jpg"
    with open(path, "wb") as f:
        f.write(request.data)
    print(f"[face] Saved: {path}")
    return '{"status": "ok"}', 200


if __name__ == "__main__":
    init_db()
    mqtt_thread = threading.Thread(target=start_mqtt, daemon=True)
    mqtt_thread.start()
    app.run(host="0.0.0.0", port=5000, debug=False)