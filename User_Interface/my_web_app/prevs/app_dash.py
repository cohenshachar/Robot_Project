from flask import Flask, jsonify, render_template
import requests
import os
from dotenv import load_dotenv

load_dotenv()
app = Flask(__name__)

FIREBASE_URL = os.getenv("FIREBASE_URL")
FIREBASE_SECRET = os.getenv("FIREBASE_SECRET")

def fetch_firebase_node(node):
    try:
        url = f"{FIREBASE_URL}/{node}.json?auth={FIREBASE_SECRET}"
        res = requests.get(url)
        res.raise_for_status()
        return res.json()
    except Exception as e:
        print("Firebase fetch error:", e)
        return {}

@app.route("/")
def dashboard():
    return render_template("dashboard.html")

@app.route("/api/motor_left")
def motor_left():
    return jsonify(fetch_firebase_node("compressed_logs/motorLeft"))

@app.route("/api/motor_right")
def motor_right():
    return jsonify(fetch_firebase_node("compressed_logs/motorRight"))

@app.route("/api/centerline")
def centerline():
    return jsonify(fetch_firebase_node("compressed_logs/CenterLine"))

@app.route("/api/odometry")
def odometry():
    return jsonify(fetch_firebase_node("compressed_logs/Odometry"))

@app.route("/api/walls")
def walls():
    return jsonify(fetch_firebase_node("compressed_logs/Walls"))

if __name__ == "__main__":
    app.run(debug=True)
