import sqlite3

DB_PATH = "/app/data/security.db"

def get_conn():
    conn = sqlite3.connect(DB_PATH, check_same_thread=False)
    conn.execute("PRAGMA journal_mode=WAL;")
    return conn


def init_db():
    conn = get_conn()
    conn.execute("""
        CREATE TABLE IF NOT EXISTS events (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            event_type TEXT NOT NULL,
            timestamp TEXT NOT NULL,
            data TEXT,
            file_path TEXT
        )
    """)
    try:
        conn.execute("ALTER TABLE events ADD COLUMN file_path TEXT")
    except Exception:
        pass
    conn.commit()
    conn.close()


def insert_event(event_type, timestamp, data, file_path=None):
    conn = get_conn()
    cursor = conn.execute(
        "INSERT INTO events (event_type, timestamp, data, file_path) VALUES (?, ?, ?, ?)",
        (event_type, timestamp, data, file_path)
    )
    conn.commit()
    row_id = cursor.lastrowid
    conn.close()
    return row_id


def get_all_events():
    conn = get_conn()
    rows = conn.execute(
        "SELECT id, event_type, timestamp, data, file_path FROM events ORDER BY id DESC"
    ).fetchall()
    conn.close()
    return rows
