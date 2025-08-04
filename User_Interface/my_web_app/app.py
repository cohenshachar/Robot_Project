from flask import Flask, render_template, request, jsonify
import requests, os
from dotenv import load_dotenv
from logic.main import solve_maze_from_web

load_dotenv()

app = Flask(__name__)

FIREBASE_URL = os.getenv("FIREBASE_URL")
FIREBASE_SECRET = os.getenv("FIREBASE_SECRET")


# ----- ROUTES -----

@app.route("/")
def home():
    return render_template("home.html")  # optional landing page

@app.route("/maze")
def maze_page():
    return render_template("index.html")

@app.route("/dashboard")
def dashboard_page():
    return render_template("dashboard.html")


# ----- MAZE SOLVER -----

@app.route("/solve", methods=["POST"])
def solve():
    data = request.get_json()
    result = solve_maze_from_web(
        maze_grid=data["maze_grid"],
        start_pos=data["start"],
        end_pos=data["end"]
    )
    if result.get("status") == "ok":
        send_to_firebase(result["directions"], data["start"], data["end"], data["maze_grid"], result.get("path"))
    return jsonify(result)

def send_to_firebase(directions, start, end, maze_grid=None, path=None):
    payload = {
        "directions": directions,
        "start": start,
        "end": end,
    }
    if maze_grid:
        payload["maze_grid"] = maze_grid

    if path:
        payload["path"] = path


    try:
        response = requests.put(
            f"{FIREBASE_URL}/solutions/latest.json?auth={FIREBASE_SECRET}", json=payload
        )
        print("✅ Sent to Firebase:", response.status_code)
    except Exception as e:
        print("❌ Error sending to Firebase:", e)


# ----- DASHBOARD API -----

def fetch_firebase_node(node):
    try:
        url = f"{FIREBASE_URL}/{node}.json?auth={FIREBASE_SECRET}"
        res = requests.get(url)
        res.raise_for_status()
        return res.json()
    except Exception as e:
        print("Firebase fetch error:", e)
        return {}

@app.route("/api/motor_left")
def motor_left():
    return jsonify(fetch_firebase_node("logs/motorLeft"))

@app.route("/api/motor_right")
def motor_right():
    return jsonify(fetch_firebase_node("logs/motorRight"))

@app.route("/api/centerline")
def centerline():
    return jsonify(fetch_firebase_node("logs/CenterLine"))

@app.route("/api/odometry")
def odometry():
    return jsonify(fetch_firebase_node("logs/Odometry"))

@app.route("/api/walls")
def walls():
    return jsonify(fetch_firebase_node("logs/Walls"))

@app.route("/api/maze_grid")
def maze_grid():
    latest = fetch_firebase_node("solutions/latest")
    return jsonify(latest.get("maze_grid", []))

@app.route("/api/maze_path")
def maze_path():
    latest = fetch_firebase_node("solutions/latest")
    return jsonify(latest.get("path", []))

@app.route("/api/maze_start")
def maze_start():
    latest = fetch_firebase_node("solutions/latest")
    return jsonify(latest.get("start", []))

@app.route("/api/maze_end")
def maze_end():
    latest = fetch_firebase_node("solutions/latest")
    return jsonify(latest.get("end", []))




if __name__ == "__main__":
    app.run(debug=True)
