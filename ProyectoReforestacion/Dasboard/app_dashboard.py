import sqlite3
import time
import json
from flask import Flask, render_template_string, jsonify
import paho.mqtt.client as mqtt

app = Flask(__name__)

# === CONFIGURACIÓN MQTT ===
BROKER = "localhost"
PORT = 1883
#usuario y contraseña del broker MQTT
MQTT_USER = "---"
MQTT_PASSWORD = "--"

# Variables globales en vivo
latest_data = {"m1": 0, "m2": 0, "m3": 0, "avg": 0, "alert": "Esperando datos..."}

# === BASE DE DATOS SQLite ===
def init_db():
    conn = sqlite3.connect('reforestacion.db')
    c = conn.cursor()
    c.execute('''CREATE TABLE IF NOT EXISTS lecturas
                 (id INTEGER PRIMARY KEY AUTOINCREMENT,
                  timestamp REAL,
                  mota1 REAL, mota2 REAL, mota3 REAL,
                  promedio REAL, alerta TEXT)''')
    conn.commit()
    conn.close()

init_db()

# === CALLBACK MQTT (Ahora recibe JSON) ===
def on_connect(client, userdata, flags, rc):
    print("[Flask] Conectado al Broker MQTT")
    client.subscribe("reforestacion/suelo/datos")

def on_message(client, userdata, msg):
    global latest_data
    try:
        payload = json.loads(msg.payload.decode('utf-8'))
        latest_data = payload
        
        # Guardar en BD
        conn = sqlite3.connect('reforestacion_v2.db')
        c = conn.cursor()
        c.execute("INSERT INTO lecturas (timestamp, mota1, mota2, mota3, promedio, alerta) VALUES (?, ?, ?, ?, ?, ?)",
                  (time.time(), payload['m1'], payload['m2'], payload['m3'], payload['avg'], payload['alert']))
        conn.commit()
        conn.close()
    except Exception as e:
        print(f"Error parseando JSON: {e}")

client = mqtt.Client(client_id="FlaskDashboardV2", protocol=mqtt.MQTTv311)
client.username_pw_set(MQTT_USER, MQTT_PASSWORD)
client.on_connect = on_connect
client.on_message = on_message
client.connect(BROKER, PORT, 60)
client.loop_start()

# === DISEÑO FRONTEND ULTRA-MODERNO ===
HTML_TEMPLATE = """
<!DOCTYPE html>
<html lang="es">
<head>
    <meta charset="UTF-8">
    <title>Dashboard Reforestación IoT</title>
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
    <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.4.0/css/all.min.css">
    <style>
        :root {
            --bg-gradient: linear-gradient(135deg, #0f2027, #203a43, #2c5364);
            --glass: rgba(255, 255, 255, 0.1);
            --glass-border: rgba(255, 255, 255, 0.2);
            --text-main: #ffffff;
            --accent-blue: #4fc3f7;
            --accent-green: #81c784;
            --accent-orange: #ffb74d;
            --accent-red: #e57373;
        }
        body {
            font-family: 'Segoe UI', system-ui, sans-serif;
            background: var(--bg-gradient);
            color: var(--text-main);
            margin: 0; padding: 30px;
            min-height: 100vh;
        }
        .header { text-align: center; margin-bottom: 30px; }
        .header h1 { font-size: 2.5em; margin: 0; letter-spacing: 2px; }
        .header p { color: var(--accent-blue); font-size: 1.2em; }
        
        .grid-container {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
            gap: 20px;
            margin-bottom: 30px;
        }
        .card {
            background: var(--glass);
            backdrop-filter: blur(10px);
            border: 1px solid var(--glass-border);
            border-radius: 15px;
            padding: 20px;
            text-align: center;
            transition: transform 0.3s ease;
        }
        .card:hover { transform: translateY(-5px); }
        .card i { font-size: 2.5em; margin-bottom: 10px; display: block; }
        .card h3 { margin: 0 0 10px 0; font-size: 1.2em; text-transform: uppercase; letter-spacing: 1px;}
        .card .value { font-size: 3em; font-weight: bold; margin: 0; }
        
        .zone1 i, .zone1 h3 { color: var(--accent-blue); }
        .zone2 i, .zone2 h3 { color: var(--accent-green); }
        .zone3 i, .zone3 h3 { color: var(--accent-orange); }
        .avg-card i, .avg-card h3 { color: #fff; }
        
        .alert-box {
            background: var(--glass);
            backdrop-filter: blur(10px);
            border-radius: 15px;
            padding: 15px 30px;
            text-align: center;
            font-size: 1.5em;
            font-weight: bold;
            margin-bottom: 30px;
            border: 2px solid var(--accent-green);
            color: var(--accent-green);
        }
        .alert-box.danger { border-color: var(--accent-red); color: var(--accent-red); }

        .chart-card {
            background: var(--glass);
            backdrop-filter: blur(10px);
            border: 1px solid var(--glass-border);
            border-radius: 15px;
            padding: 20px;
        }
    </style>
</head>
<body>
    <div class="header">
        <h1><i class="fa-solid fa-tree"></i> ECO-WATCH: RED DE MONITOREO</h1>
        <p>Restauración Ecológica & IoT</p>
    </div>

    <div id="alertContainer" class="alert-box">
        <i class="fa-solid fa-shield-halved"></i> <span id="alertText">SISTEMA INICIANDO...</span>
    </div>

    <div class="grid-container">
        <div class="card zone1">
            <i class="fa-solid fa-seedling"></i>
            <h3>Zona A (Mota 1)</h3>
            <p class="value" id="m1Val">0%</p>
        </div>
        <div class="card zone2">
            <i class="fa-solid fa-leaf"></i>
            <h3>Zona B (Mota 2)</h3>
            <p class="value" id="m2Val">0%</p>
        </div>
        <div class="card zone3">
            <i class="fa-solid fa-spa"></i>
            <h3>Zona C (Mota 3)</h3>
            <p class="value" id="m3Val">0%</p>
        </div>
        <div class="card avg-card">
            <i class="fa-solid fa-chart-line"></i>
            <h3>Promedio Global</h3>
            <p class="value" id="avgVal">0%</p>
        </div>
    </div>

    <div class="chart-card">
        <canvas id="mainChart" height="100"></canvas>
    </div>

    <script>
        const ctx = document.getElementById('mainChart').getContext('2d');
        const chart = new Chart(ctx, {
            type: 'line',
            data: {
                labels: [],
                datasets: [
                    { label: 'Zona A', data: [], borderColor: '#4fc3f7', borderWidth: 2, tension: 0.4 },
                    { label: 'Zona B', data: [], borderColor: '#81c784', borderWidth: 2, tension: 0.4 },
                    { label: 'Zona C', data: [], borderColor: '#ffb74d', borderWidth: 2, tension: 0.4 },
                    { label: 'Promedio', data: [], borderColor: '#ffffff', borderWidth: 3, borderDash: [5, 5], tension: 0.4 }
                ]
            },
            options: {
                responsive: true,
                plugins: { legend: { labels: { color: 'white' } } },
                scales: {
                    x: { ticks: { color: 'white' }, grid: { color: 'rgba(255,255,255,0.1)' } },
                    y: { min: 0, max: 100, ticks: { color: 'white' }, grid: { color: 'rgba(255,255,255,0.1)' } }
                }
            }
        });

        async function fetchData() {
            const res = await fetch('/api/data');
            const data = await res.json();

            document.getElementById('m1Val').innerText = data.latest.m1 + '%';
            document.getElementById('m2Val').innerText = data.latest.m2 + '%';
            document.getElementById('m3Val').innerText = data.latest.m3 + '%';
            document.getElementById('avgVal').innerText = data.latest.avg + '%';
            
            const alertDiv = document.getElementById('alertContainer');
            const alertSpan = document.getElementById('alertText');
            alertSpan.innerText = data.latest.alert;
            if(data.latest.alert.includes("ALERTA")) {
                alertDiv.className = 'alert-box danger';
            } else {
                alertDiv.className = 'alert-box';
            }

            chart.data.labels = data.labels;
            chart.data.datasets[0].data = data.values_m1;
            chart.data.datasets[1].data = data.values_m2;
            chart.data.datasets[2].data = data.values_m3;
            chart.data.datasets[3].data = data.values_avg;
            chart.update();
        }

        fetchData();
        setInterval(fetchData, 1500);
    </script>
</body>
</html>
"""

@app.route('/')
def index():
    return render_template_string(HTML_TEMPLATE)

@app.route('/api/data')
def get_data():
    conn = sqlite3.connect('reforestacion_v2.db')
    c = conn.cursor()
    # Obtener últimas 30 lecturas
    c.execute("SELECT timestamp, mota1, mota2, mota3, promedio FROM lecturas ORDER BY id DESC LIMIT 30")
    rows = c.fetchall()
    conn.close()

    labels = [time.strftime('%H:%M:%S', time.localtime(row[0])) for row in reversed(rows)]
    values_m1 = [row[1] for row in reversed(rows)]
    values_m2 = [row[2] for row in reversed(rows)]
    values_m3 = [row[3] for row in reversed(rows)]
    values_avg = [row[4] for row in reversed(rows)]

    return jsonify({
        "latest": latest_data,
        "labels": labels,
        "values_m1": values_m1,
        "values_m2": values_m2,
        "values_m3": values_m3,
        "values_avg": values_avg
    })

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000, debug=False)