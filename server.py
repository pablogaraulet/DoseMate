
from flask import Flask, request, render_template
import sqlite3
import datetime

app = Flask(__name__)

def init_db():
    conn = sqlite3.connect('medication.db')
    c = conn.cursor()
    c.execute('''
        CREATE TABLE IF NOT EXISTS logs (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            device_id TEXT NOT NULL,
            type TEXT NOT NULL,
            temperature REAL,
            humidity REAL,
            status TEXT NOT NULL,
            timestamp TEXT NOT NULL
        )
    ''')
    conn.commit()
    conn.close()

@app.route("/medication")
def receive_medication():
    device_id = request.args.get("device_id", "unknown")
    status = request.args.get("status", "unknown")
    timestamp = request.args.get("timestamp", datetime.datetime.utcnow().isoformat())

    conn = sqlite3.connect('medication.db')
    c = conn.cursor()
    c.execute("INSERT INTO logs (device_id, type, status, timestamp) VALUES (?, ?, ?, ?)",
              (device_id, "medication", status, timestamp))
    conn.commit()
    conn.close()

    print(f"[LOG] Device {device_id} medication status '{status}' at {timestamp}")
    return f"Medication status '{status}' received"

@app.route("/envlog")
def receive_environment():
    device_id = request.args.get("device_id", "unknown")
    temperature = request.args.get("temperature")
    humidity = request.args.get("humidity")
    status = request.args.get("status", "unknown")
    timestamp = request.args.get("timestamp", datetime.datetime.utcnow().isoformat())

    conn = sqlite3.connect('medication.db')
    c = conn.cursor()
    c.execute("INSERT INTO logs (device_id, type, temperature, humidity, status, timestamp) VALUES (?, ?, ?, ?, ?, ?)",
              (device_id, "environment", temperature, humidity, status, timestamp))
    conn.commit()
    conn.close()

    print(f"[ENV] Device {device_id} environment {temperature}°C / {humidity}% -> {status} at {timestamp}")
    return f"Environment data received"

@app.route("/dashboard")
def dashboard():
    conn = sqlite3.connect('medication.db')
    c = conn.cursor()
    c.execute("SELECT * FROM logs ORDER BY timestamp DESC")
    rows = c.fetchall()
    conn.close()
    return render_template("dashboard.html", logs=rows)

@app.route("/")
def home():
    return render_template("index.html")

@app.route("/contact")
def contact():
    return render_template("contact.html")

if __name__ == '__main__':
    init_db()
    app.run(host='0.0.0.0', port=5000)
