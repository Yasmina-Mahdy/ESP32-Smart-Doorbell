import os
import threading
from datetime import datetime
from flask import Flask, render_template, request, jsonify, send_file
from flask_socketio import SocketIO
from database import init_db, get_all_events, insert_event
from mqtt_client import start_mqtt, set_socketio

app = Flask(__name__)
app.config["SECRET_KEY"] = "securewatchkey"
socketio = SocketIO(app, cors_allowed_origins="*")

UPLOAD_IMAGE_DIR = "uploads/images"
UPLOAD_AUDIO_DIR = "uploads/audio"

os.makedirs(UPLOAD_IMAGE_DIR, exist_ok=True)
os.makedirs(UPLOAD_AUDIO_DIR, exist_ok=True)


def get_timestamp(req):
    ts = req.args.get("timestamp")
    if not ts:
        ts = datetime.now().strftime("%I:%M:%S %p %d/%m/%Y")
    return ts


def emit_event(row_id, event_type, timestamp, data, file_path):
    socketio.emit("new_event", {
        "id": row_id,
        "event_type": event_type,
        "timestamp": timestamp,
        "data": data,
        "file_path": file_path
    })


@app.route("/")
def dashboard():
    rows = get_all_events()
    return render_template("dashboard.html", rows=rows)


@app.route("/media/<path:filepath>")
def serve_media(filepath):
    return send_file(filepath)


@app.route("/upload/image", methods=["POST"])
def upload_image():
    data = request.data
    if not data:
        return jsonify({"error": "No data received"}), 400

    timestamp = get_timestamp(request)
    filename = datetime.now().strftime("%Y%m%d_%H%M%S") + ".jpg"
    filepath = os.path.join(UPLOAD_IMAGE_DIR, filename)

    with open(filepath, "wb") as f:
        f.write(data)

    row_id = insert_event("image", timestamp, "Image captured", file_path=filepath)
    emit_event(row_id, "image", timestamp, "Image captured", filepath)
    return jsonify({"status": "ok", "file": filepath, "timestamp": timestamp}), 200


@app.route("/upload/audio", methods=["POST"])
def upload_audio():
    data = request.data
    if not data:
        return jsonify({"error": "No data received"}), 400

    timestamp = get_timestamp(request)
    filename = datetime.now().strftime("%Y%m%d_%H%M%S") + ".wav"
    filepath = os.path.join(UPLOAD_AUDIO_DIR, filename)

    with open(filepath, "wb") as f:
        f.write(data)

    row_id = insert_event("audio", timestamp, "Audio recorded", file_path=filepath)
    emit_event(row_id, "audio", timestamp, "Audio recorded", filepath)
    return jsonify({"status": "ok", "file": filepath, "timestamp": timestamp}), 200


if __name__ == "__main__":
    init_db()
    set_socketio(socketio)
    mqtt_thread = threading.Thread(target=start_mqtt, daemon=True)
    mqtt_thread.start()
    socketio.run(app, host="0.0.0.0", port=5000, allow_unsafe_werkzeug=True)
