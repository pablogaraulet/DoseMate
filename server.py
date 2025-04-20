# Flask backend with SQLite database
# File: server.py

from flask import Flask, request, jsonify, render_template
import sqlite3
import datetime

app = Flask(__name__)

# --- Initialize the database (only needed once) ---
def init_db():
    conn = sqlite3.connect('medication.db')
    c = conn.cursor()
    c.execute('''
        CREATE TABLE IF NOT EXISTS medication_logs (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            device_id TEXT NOT NULL,
            status TEXT NOT NULL,
            timestamp TEXT NOT NULL
        )
    ''')
    conn.commit()
    conn.close()

# --- Route to receive medication confirmation ---
@app.route("/medication")
def receive_medication():
    device_id = request.args.get("device_id", "unknown")
    status = request.args.get("status", "unknown")
    timestamp = request.args.get("timestamp", datetime.datetime.utcnow().isoformat())

    conn = sqlite3.connect('medication.db')
    c = conn.cursor()
    c.execute("INSERT INTO medication_logs (device_id, status, timestamp) VALUES (?, ?, ?)",
              (device_id, status, timestamp))
    conn.commit()
    conn.close()

    print(f"[LOG] Device {device_id} reported status '{status}' at {timestamp}")
    return f"Received status '{status}' from device {device_id}"

# --- Route to render the dashboard using template ---
@app.route("/dashboard")
def dashboard():
    conn = sqlite3.connect('medication.db')
    c = conn.cursor()
    c.execute("SELECT * FROM medication_logs ORDER BY timestamp DESC")
    rows = c.fetchall()
    conn.close()
    return render_template("dashboard.html", logs=rows)

# --- Home route with styled index ---
@app.route("/")
def home():
    return render_template("index.html")

# --- About page ---
@app.route("/about")
def about():
    return render_template("about.html")

# --- Start the Flask server ---
if __name__ == '__main__':
    init_db()
    app.run(host='0.0.0.0', port=5000)

