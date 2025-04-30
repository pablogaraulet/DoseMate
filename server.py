# Flask backend with SQLite database
from flask import Flask, request, render_template, redirect, url_for, session, flash
from werkzeug.security import generate_password_hash, check_password_hash
import sqlite3
import datetime
from functools import wraps

app = Flask(__name__)
app.secret_key = "cambia_esto_por_una_clave_muy_segura"

def init_db():
    conn = sqlite3.connect('medication.db')
    c = conn.cursor()
    c.execute('''
        CREATE TABLE IF NOT EXISTS medication_logs (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            device_id TEXT NOT NULL,
            type TEXT,
            temperature REAL,
            humidity REAL,
            status TEXT NOT NULL,
            timestamp TEXT NOT NULL
        )
    ''')
    c.execute('''
        CREATE TABLE IF NOT EXISTS users (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            username TEXT UNIQUE NOT NULL,
            email TEXT UNIQUE NOT NULL,
            password_hash TEXT NOT NULL,
            created_at TEXT NOT NULL
        )
    ''')
    profile_cols = {
        "age": "INTEGER",
        "weight": "REAL",
        "gender": "TEXT",
        "medications": "TEXT",
        "allergies": "TEXT"
    }
    for col, col_type in profile_cols.items():
        try:
            c.execute(f"ALTER TABLE users ADD COLUMN {col} {col_type}")
        except sqlite3.OperationalError:
            pass
    conn.commit()
    conn.close()

def login_required(f):
    @wraps(f)
    def decorated(*args, **kwargs):
        if "user_id" not in session:
            flash("Tienes que iniciar sesión primero", "error")
            return redirect(url_for("login"))
        return f(*args, **kwargs)
    return decorated

@app.route("/medication")
def receive_medication():
    device_id = request.args.get("device_id", "unknown")
    status = request.args.get("status", "unknown")
    timestamp = request.args.get("timestamp", datetime.datetime.utcnow().isoformat())

    conn = sqlite3.connect('medication.db')
    c = conn.cursor()
    c.execute(
        "INSERT INTO medication_logs (device_id, type, temperature, humidity, status, timestamp) "
        "VALUES (?, 'medication', NULL, NULL, ?, ?)",
        (device_id, status, timestamp)
    )
    conn.commit()
    conn.close()
    print(f"[LOG] Device {device_id} reported status '{status}' at {timestamp}")
    return f"Received status '{status}' from device {device_id}"

@app.route("/envlog")
def receive_environment():
    device_id = request.args.get("device_id", "unknown")
    temp = request.args.get("temperature", type=float)
    hum = request.args.get("humidity", type=float)
    status = request.args.get("status", "unknown")
    timestamp = request.args.get("timestamp", datetime.datetime.utcnow().isoformat())

    conn = sqlite3.connect('medication.db')
    c = conn.cursor()
    c.execute(
        "INSERT INTO medication_logs (device_id, type, temperature, humidity, status, timestamp) "
        "VALUES (?, 'environment', ?, ?, ?, ?)",
        (device_id, temp, hum, status, timestamp)
    )
    conn.commit()
    conn.close()
    print(f"[ENV] {device_id} – {temp}°C / {hum}% → {status} at {timestamp}")
    return "Environment data received"

@app.route("/signup", methods=["GET", "POST"])
def signup():
    if request.method == "POST":
        username = request.form["username"]
        email = request.form["email"]
        pwd = request.form["password"]
        pwd2 = request.form["confirm_password"]
        if pwd != pwd2:
            flash("Las contraseñas no coinciden", "error")
            return redirect(url_for("signup"))
        hash_pwd = generate_password_hash(pwd)
        created = datetime.datetime.utcnow().isoformat()
        try:
            conn = sqlite3.connect('medication.db')
            c = conn.cursor()
            c.execute(
                "INSERT INTO users (username, email, password_hash, created_at) VALUES (?, ?, ?, ?)",
                (username, email, hash_pwd, created)
            )
            conn.commit()
            conn.close()
            flash("Usuario creado correctamente. Ya puedes iniciar sesión.", "success")
            return redirect(url_for("login"))
        except sqlite3.IntegrityError:
            flash("El usuario o email ya existe.", "error")
            return redirect(url_for("signup"))
    return render_template("signup.html")

@app.route("/login", methods=["GET", "POST"])
def login():
    if request.method == "POST":
        username = request.form["username"]
        pwd = request.form["password"]
        conn = sqlite3.connect('medication.db')
        c = conn.cursor()
        c.execute("SELECT id, password_hash FROM users WHERE username = ?", (username,))
        user = c.fetchone()
        conn.close()
        if user and check_password_hash(user[1], pwd):
            session["user_id"] = user[0]
            session["username"] = username
            flash(f"¡Bienvenido, {username}!", "success")
            return redirect(url_for("welcome"))
        else:
            flash("Usuario o contraseña incorrectos", "error")
            return redirect(url_for("login"))
    return render_template("login.html")

@app.route("/logout")
def logout():
    session.clear()
    flash("Has cerrado sesión.", "info")
    return redirect(url_for("home"))

@app.route("/welcome")
@login_required
def welcome():
    return render_template("index2.html", username=session.get("username"))

@app.route("/profile")
@login_required
def profile():
    uid = session["user_id"]
    conn = sqlite3.connect('medication.db')
    c = conn.cursor()
    c.execute("SELECT age, weight, gender, medications, allergies FROM users WHERE id = ?", (uid,))
    row = c.fetchone()
    conn.close()
    data = {k: (row[i] or "") for i, k in enumerate(["age", "weight", "gender", "medications", "allergies"])}
    return render_template("profile.html", edit=False, **data)

@app.route("/profile/edit", methods=["GET", "POST"])
@login_required
def edit_profile():
    uid = session["user_id"]
    conn = sqlite3.connect('medication.db')
    c = conn.cursor()
    if request.method == "POST":
        vals = (
            request.form.get("age"),
            request.form.get("weight"),
            request.form.get("gender"),
            request.form.get("medications"),
            request.form.get("allergies"),
            uid
        )
        c.execute(
            "UPDATE users SET age=?, weight=?, gender=?, medications=?, allergies=? WHERE id=?",
            vals
        )
        conn.commit()
        conn.close()
        flash("Profile updated successfully!", "success")
        return redirect(url_for("profile"))
    c.execute("SELECT age, weight, gender, medications, allergies FROM users WHERE id = ?", (uid,))
    row = c.fetchone()
    conn.close()
    data = {k: (row[i] or "") for i, k in enumerate(["age", "weight", "gender", "medications", "allergies"])}
    return render_template("profile.html", edit=True, **data)

@app.route("/dashboard")
@login_required
def dashboard():
    # Abrir la conexión y configurar row_factory para tuplas tipo dict
    conn = sqlite3.connect('medication.db')
    conn.row_factory = sqlite3.Row
    c = conn.cursor()
    c.execute("""
        SELECT
            id,
            device_id,
            type,
            temperature,
            humidity,
            status,
            timestamp
        FROM medication_logs
        ORDER BY timestamp DESC
    """)
    rows = c.fetchall()
    conn.close()
    return render_template("dashboard.html", logs=rows)

@app.route("/stats")
@login_required
def stats():
    conn = sqlite3.connect('medication.db')
    c = conn.cursor()
    c.execute("SELECT COUNT(*) FROM medication_logs")
    total_records = c.fetchone()[0]
    c.execute("SELECT COUNT(DISTINCT device_id) FROM medication_logs")
    unique_devices = c.fetchone()[0]
    c.execute("SELECT AVG(temperature) FROM medication_logs WHERE type='environment'")
    avg_temperature = c.fetchone()[0] or 0
    c.execute("SELECT AVG(humidity) FROM medication_logs WHERE type='environment'")
    avg_humidity = c.fetchone()[0] or 0
    c.execute("""
        SELECT timestamp, temperature, humidity
        FROM medication_logs
        WHERE type='environment'
        ORDER BY timestamp ASC
    """)
    time_series = c.fetchall()
    conn.close()

    return render_template(
        "stats.html",
        total_records=total_records,
        unique_devices=unique_devices,
        avg_temperature=round(avg_temperature, 2),
        avg_humidity=round(avg_humidity, 2),
        time_series=time_series
    )

@app.route("/")
def home():
    return render_template("index.html")

@app.route("/contact")
def contact():
    return render_template("contact.html")

if __name__ == '__main__':
    init_db()
    app.run(host='0.0.0.0', port=5000, debug=True)
