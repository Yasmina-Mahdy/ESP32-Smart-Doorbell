import threading
from flask import Flask, render_template
from database import init_db, get_all_events
from mqtt_client import start_mqtt

app = Flask(__name__)


@app.route("/")
def dashboard():
    rows = get_all_events()
    return render_template("dashboard.html", rows=rows)


if __name__ == "__main__":
    init_db()
    mqtt_thread = threading.Thread(target=start_mqtt, daemon=True)
    mqtt_thread.start()
    app.run(host="0.0.0.0", port=5000, debug=False)