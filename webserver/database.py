import sqlite3

DB_PATH = "security.db"


def init_db():
    conn = sqlite3.connect(DB_PATH, check_same_thread=False)
    conn.execute("""
        CREATE TABLE IF NOT EXISTS events (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            event_type TEXT NOT NULL,
            timestamp TEXT NOT NULL,
            data TEXT
        )
    """)
    conn.commit()
    conn.close()


def insert_event(event_type, timestamp, data):
    conn = sqlite3.connect(DB_PATH, check_same_thread=False)
    conn.execute(
        "INSERT INTO events (event_type, timestamp, data) VALUES (?, ?, ?)",
        (event_type, timestamp, data)
    )
    conn.commit()
    conn.close()


def get_all_events():
    conn = sqlite3.connect(DB_PATH, check_same_thread=False)
    rows = conn.execute(
        "SELECT id, event_type, timestamp, data FROM events ORDER BY id DESC"
    ).fetchall()
    conn.close()
    return rows