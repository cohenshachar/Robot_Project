import streamlit as st
import pandas as pd
import json
import matplotlib.pyplot as plt
import matplotlib.dates as mdates
import os
import requests
from dotenv import load_dotenv
from datetime import datetime
from streamlit_autorefresh import st_autorefresh

# Load secrets from .env
load_dotenv()
FIREBASE_URL = os.getenv("FIREBASE_URL")
FIREBASE_SECRET = os.getenv("FIREBASE_SECRET")

st.set_page_config(page_title="🤖 Robot Live Dashboard", layout="wide")
st.title("🤖 Live Robot Motion Dashboard (from Firebase)")


# ---------------- FIREBASE FETCHERS ---------------- #

def fetch_firebase_node(node: str):
    try:
        url = f"{FIREBASE_URL}/{node}.json?auth={FIREBASE_SECRET}"
        res = requests.get(url)
        res.raise_for_status()
        return res.json()
    except Exception as e:
        st.warning(f"Could not fetch `{node}` from Firebase: {e}")
        return {}

def parse_string_log(raw_dict):
    if raw_dict is None or not isinstance(raw_dict, dict):
        return pd.DataFrame()
    rows = []
    for session_id, entries in raw_dict.items():
        if isinstance(entries, list):
            for item in entries:
                try:
                    if isinstance(item, str):
                        cleaned = item.strip().rstrip(",").rstrip("\r")
                        parsed = json.loads(cleaned)
                    elif isinstance(item, dict):
                        parsed = item
                    else:
                        continue
                    parsed["session"] = session_id
                    rows.append(parsed)
                except:
                    continue
    df = pd.DataFrame(rows)
    df.columns = [col.strip().lower() for col in df.columns]
    if "line_x" in df.columns and "line_y" in df.columns:
        df = df.dropna(subset=["line_x", "line_y"])
    return df

def parse_motor_log(raw_dict):
    if not raw_dict:
        return pd.DataFrame()
    records = []
    for session_id, entries in raw_dict.items():
        if not isinstance(entries, list):
            continue
        for raw_str in entries:
            try:
                json_str = raw_str.strip().replace('\r', '')
                data = json.loads(json_str)
                data["session"] = session_id
                records.append(data)
            except:
                continue
    df = pd.DataFrame(records)
    try:
        df["time_numeric"] = pd.to_datetime(df["time"], format="%H-%M-%S-%f", errors="coerce")
        df = df.dropna(subset=["time_numeric"])
    except:
        pass
    return df

def parse_wall_log(raw_dict):
    if not raw_dict:
        return pd.DataFrame()
    records = []
    for session_id, entries in raw_dict.items():
        if not isinstance(entries, list):
            continue
        for raw_str in entries:
            try:
                json_str = raw_str.strip().replace('\r', '')
                data = json.loads(json_str)
                data["session"] = session_id
                records.append(data)
            except:
                continue
    df = pd.DataFrame(records)
    for col in ["lWall_x", "lWall_y", "rWall_x", "rWall_y"]:
        if col in df.columns:
            df[col] = pd.to_numeric(df[col], errors="coerce")
    return df.dropna(subset=["lWall_x", "lWall_y", "rWall_x", "rWall_y"])

# ---------------- CACHED LOADERS ---------------- #

@st.cache_data(ttl=30)
def load_motor_left():
    return parse_motor_log(fetch_firebase_node("compressed_logs/motorLeft"))

@st.cache_data(ttl=30)
def load_motor_right():
    return parse_motor_log(fetch_firebase_node("compressed_logs/motorRight"))

@st.cache_data(ttl=30)
def load_odometry():
    return parse_string_log(fetch_firebase_node("compressed_logs/Odometry"))

@st.cache_data(ttl=30)
def load_center():
    return parse_string_log(fetch_firebase_node("compressed_logs/CenterLine"))

@st.cache_data(ttl=30)
def load_walls():
    return parse_wall_log(fetch_firebase_node("compressed_logs/Walls"))

# ---------------- SIDEBAR CONTROLS ---------------- #
with st.sidebar:
    st.markdown("## ⚙️ Controls")

    refresh_interval = st.slider("Refresh Interval (seconds)", 5, 60, value=30)
    st.session_state["refresh_interval"] = refresh_interval

    if st.button("🔁 Force Refresh Now"):
        load_motor_left.clear()
        load_motor_right.clear()
        load_odometry.clear()
        load_center.clear()
        load_walls.clear()
        st.rerun()

    st.caption("Live robot dashboard with Firebase.")
    

# 🔁 Background auto-refresh using selected interval
st_autorefresh(interval=st.session_state.get("refresh_interval", 30) * 1000, key="quiet-refresh")

# ---------------- LOAD DATA ---------------- #
with st.spinner("📱 Loading Firebase data..."):
    df_motor_left = load_motor_left()
    df_motor_right = load_motor_right()
    df_odometry = load_odometry()
    df_center = load_center()
    df_walls = load_walls()

st.caption(f"🔁 Last updated: {pd.Timestamp.now().strftime('%H:%M:%S')}")

# ---------------- PATH VISUALIZATION ---------------- #
col1, col2 = st.columns(2)
with col1:
    st.markdown("### 📜 Robot Path Visualization")
    if not df_odometry.empty or not df_center.empty or not df_walls.empty:
        fig, ax = plt.subplots(figsize=(7, 6))
        if {"x", "y"}.issubset(df_odometry.columns):
            ax.plot(df_odometry["x"], df_odometry["y"], label="Odometry", marker='o', linestyle='-', color='blue')
        if {"line_x", "line_y"}.issubset(df_center.columns):
            ax.plot(df_center["line_x"], df_center["line_y"], label="Center Line", marker='x', linestyle='--', color='green')
        if {"lWall_x", "lWall_y"}.issubset(df_walls.columns):
            ax.scatter(df_walls["lWall_x"], df_walls["lWall_y"], label="Left Wall", marker='^', color='purple', alpha=0.6)
        if {"rWall_x", "rWall_y"}.issubset(df_walls.columns):
            ax.scatter(df_walls["rWall_x"], df_walls["rWall_y"], label="Right Wall", marker='v', color='orange', alpha=0.6)
        ax.set_xlabel("X (cm)")
        ax.set_ylabel("Y (cm)")
        ax.set_title("Path, Center Line & Walls")
        ax.grid(True)
        ax.axis("equal")
        ax.legend()
        st.pyplot(fig)
    else:
        st.info("Odometry or wall data not available yet.")



# ---------------- MOTOR GRAPHS ---------------- #
st.markdown("### 🚞 Motor PWM Comparison")
motor_col1, motor_col2 = st.columns(2)

def render_motor_plot(df_motor, title, color):
    if not df_motor.empty and {"time", "pwm"}.issubset(df_motor.columns):
        df_motor["time_numeric"] = pd.to_datetime(df_motor["time"], format="%H-%M-%S-%f", errors="coerce")
        df_motor = df_motor.dropna(subset=["time_numeric"])
        df_motor["pwm"] = pd.to_numeric(df_motor["pwm"], errors="coerce")
        df_motor = df_motor.dropna(subset=["pwm"])
        df_motor = df_motor.sort_values("time_numeric")
        fig, ax = plt.subplots(figsize=(6, 3))
        ax.plot(df_motor["time_numeric"], df_motor["pwm"], color=color)
        ax.set_xlabel("Time")
        ax.set_ylabel("PWM")
        ax.set_title(title)
        ax.grid(True)
        ax.xaxis.set_major_formatter(mdates.DateFormatter('%H:%M:%S'))
        fig.autofmt_xdate(rotation=45)
        st.pyplot(fig)
    else:
        st.warning(f"{title} data missing or malformed.")

def render_rpm_plot(df_motor, title):
    if not df_motor.empty and {"time", "input", "target"}.issubset(df_motor.columns):
        df_motor["time_numeric"] = pd.to_datetime(df_motor["time"], format="%H-%M-%S-%f", errors="coerce")
        df_motor = df_motor.dropna(subset=["time_numeric"])
        df_motor = df_motor.sort_values("time_numeric")
        df_motor["input"] = pd.to_numeric(df_motor["input"], errors="coerce")
        df_motor["target"] = pd.to_numeric(df_motor["target"], errors="coerce")
        fig, ax = plt.subplots(figsize=(6, 3))
        ax.plot(df_motor["time_numeric"], df_motor["input"], label="Current RPM", color="steelblue")
        ax.plot(df_motor["time_numeric"], df_motor["target"], label="Target RPM", color="orange")
        ax.set_xlabel("Time")
        ax.set_ylabel("RPM")
        ax.set_title(title)
        ax.legend()
        ax.grid(True)
        ax.xaxis.set_major_formatter(mdates.DateFormatter('%H:%M:%S'))
        fig.autofmt_xdate(rotation=45)
        st.pyplot(fig)

with motor_col1:
    st.markdown("#### 🔴 MotorLeft PWM")
    render_motor_plot(df_motor_left, "MotorLeft PWM", "crimson")
    render_rpm_plot(df_motor_left, "MotorLeft RPM")

with motor_col2:
    st.markdown("#### 🔵 MotorRight PWM")
    render_motor_plot(df_motor_right, "MotorRight PWM", "darkorange")
    render_rpm_plot(df_motor_right, "MotorRight RPM")
