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
    # Nueva tabla para logs ambientales
    c.execute('''
        CREATE TABLE IF NOT EXISTS environment_logs (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            device_id TEXT NOT NULL,
            temperature REAL,
            humidity REAL,
            status TEXT,
            timestamp TEXT
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

# --- Route to receive environmental data ---
@app.route("/envlog")
def receive_environment():
    device_id = request.args.get("device_id", "unknown")
    temperature = request.args.get("temperature")
    humidity = request.args.get("humidity")
    status = request.args.get("status", "unknown")
    timestamp = request.args.get("timestamp", datetime.datetime.utcnow().isoformat())

    conn = sqlite3.connect('medication.db')
    c = conn.cursor()
    c.execute("INSERT INTO environment_logs (device_id, temperature, humidity, status, timestamp) VALUES (?, ?, ?, ?, ?)",
              (device_id, temperature, humidity, status, timestamp))
    conn.commit()
    conn.close()

    print(f"[ENV] Device {device_id} reported {temperature}°C / {humidity}% → {status} at {timestamp}")
    return f"Logged environmental data from {device_id}"

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

# --- Contact page ---
@app.route("/contact")
def contact():
    return render_template("contact.html")

# --- Start the Flask server ---
if __name__ == '__main__':
    init_db()
    app.run(host='0.0.0.0', port=5000)
