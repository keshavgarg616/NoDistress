import base64
import os
import tempfile
import threading
import time
import requests
from collections import deque

from flask import Flask, jsonify, request
from CV.ExpressionDetector.emotion_detection import get_emotion_distress_score
from CV.PostureDetector.pose_detection import get_posture_stats

app = Flask(__name__)

# Holding latest result in memory for get requests to frontend
latest_result = {
    "image": None,  # base64 encoded image
    "emotion": None,
    "posture": None,
}

ESP32_IP = os.environ.get(
    "ESP32_IP", "10.244.188.38"
)  # Can be set via environment variable
tracking_state = "face"  # "body" or "face"

# --- Feature Flags ---
ENABLE_BODY_FRAMING = True
ENABLE_FACE_FRAMING = True

# --- Rolling Score History ---
POSTURE_HISTORY = deque(maxlen=3)
EMOTION_HISTORY = deque(maxlen=3)


def background_task():
    global latest_result, tracking_state
    while True:
        try:
            # Fetch the single frame from ESP32
            response = requests.get(f"http://{ESP32_IP}/capture", timeout=10)
            if response.status_code == 200:
                image_bytes = response.content

                # Save to temp file
                temp_file = tempfile.NamedTemporaryFile(delete=False, suffix=".jpg")
                temp_file.write(image_bytes)
                temp_file.close()

                try:
                    try:
                        emotion_result = get_emotion_distress_score(temp_file.name)
                        e_score = (
                            emotion_result.get("score", 0)
                            if emotion_result and "score" in emotion_result
                            else 0
                        )
                        e_label = (
                            emotion_result.get("dominant_emotion", "unknown")
                            if emotion_result
                            else "unknown"
                        )
                    except Exception as e:
                        print(f"Emotion evaluation error: {e}")
                        emotion_result = None
                        e_score = 0
                        e_label = "Error"

                    try:
                        posture_result = get_posture_stats(temp_file.name)
                        p_score = (
                            posture_result.get("posture_score", 0)
                            if posture_result and "posture_score" in posture_result
                            else 0
                        )
                        p_label = (
                            posture_result.get("posture_label", "unknown")
                            if posture_result
                            else "unknown"
                        )
                    except Exception as e:
                        print(f"Posture evaluation error: {e}")
                        posture_result = None
                        p_score = 0
                        p_label = "Body not visible"

                    # --- Rolling Score Computation ---
                    if p_label != "Body not visible" and p_label != "unknown":
                        POSTURE_HISTORY.append(p_score)
                    if e_label != "Error" and e_label != "unknown":
                        EMOTION_HISTORY.append(e_score)

                    avg_p_score = (
                        sum(POSTURE_HISTORY) / len(POSTURE_HISTORY)
                        if POSTURE_HISTORY
                        else 0
                    )
                    avg_e_score = (
                        sum(EMOTION_HISTORY) / len(EMOTION_HISTORY)
                        if EMOTION_HISTORY
                        else 0
                    )

                    # --- Auto-framing logic (State Machine) ---
                    # Override state machine based on global toggles
                    if ENABLE_BODY_FRAMING and not ENABLE_FACE_FRAMING:
                        tracking_state = "body"
                    elif ENABLE_FACE_FRAMING and not ENABLE_BODY_FRAMING:
                        tracking_state = "face"
                    elif not ENABLE_BODY_FRAMING and not ENABLE_FACE_FRAMING:
                        tracking_state = "none"

                    print(f"--- Current Mode: {tracking_state.upper()} TRACKING ---")

                    center_pos = (
                        posture_result.get("center_position")
                        if posture_result
                        else None
                    )
                    face_width = (
                        emotion_result.get("face_width", 0) if emotion_result else 0
                    )

                    moved_x = False

                    # Only pan if framing is enabled for current state
                    is_framing_enabled = (
                        tracking_state == "body" and ENABLE_BODY_FRAMING
                    ) or (tracking_state == "face" and ENABLE_FACE_FRAMING)

                    if center_pos is not None and is_framing_enabled:
                        nose_x = center_pos["x"]
                        # The image is a 3x3 grid. Left is < 0.33, Right is > 0.66
                        if nose_x > 0.66:
                            print(f"Auto-framing: Moving LEFT (nose_x: {nose_x:.2f})")
                            requests.get(f"http://{ESP32_IP}/action?go=L", timeout=2)
                            time.sleep(0.2)  # Turn slightly
                            requests.get(f"http://{ESP32_IP}/action?go=S", timeout=2)
                            moved_x = True
                        elif nose_x < 0.33:
                            print(f"Auto-framing: Moving RIGHT (nose_x: {nose_x:.2f})")
                            requests.get(f"http://{ESP32_IP}/action?go=R", timeout=2)
                            time.sleep(0.2)  # Turn slightly
                            requests.get(f"http://{ESP32_IP}/action?go=S", timeout=2)
                            moved_x = True

                    # Only adjust distance if we didn't just turn, to avoid overwhelming the motors
                    if not moved_x:
                        if tracking_state == "body":
                            if p_label == "Body not visible":
                                if ENABLE_BODY_FRAMING:
                                    print(
                                        "Auto-framing (Body): Moving BACKWARD to see full body"
                                    )
                                    requests.get(
                                        f"http://{ESP32_IP}/action?go=B", timeout=2
                                    )
                                    time.sleep(0.3)
                                    requests.get(
                                        f"http://{ESP32_IP}/action?go=S", timeout=2
                                    )
                            else:
                                # Body is fully visible, check for distress
                                if avg_p_score > 0.7:
                                    print(
                                        f"Posture distress detected (avg score: {avg_p_score:.2f} > 0.7)! Switching to FACE framing."
                                    )
                                    tracking_state = "face"

                        elif tracking_state == "face":
                            if face_width > 0 and face_width < 200:
                                # Face is small (< 1/8th of 2048px), meaning person is far away
                                if ENABLE_FACE_FRAMING:
                                    print(
                                        f"Auto-framing (Face): Moving FORWARD to get closer to face (width: {face_width})"
                                    )
                                    requests.get(
                                        f"http://{ESP32_IP}/action?go=F", timeout=2
                                    )
                                    time.sleep(0.3)
                                    requests.get(
                                        f"http://{ESP32_IP}/action?go=S", timeout=2
                                    )
                            elif face_width > 400:
                                # Face is large (> 1/3rd of 2048px), meaning person is too close
                                if ENABLE_FACE_FRAMING:
                                    print(
                                        f"Auto-framing (Face): Moving BACKWARD (face_width: {face_width})"
                                    )
                                    requests.get(
                                        f"http://{ESP32_IP}/action?go=B", timeout=2
                                    )
                                    time.sleep(0.3)
                                    requests.get(
                                        f"http://{ESP32_IP}/action?go=S", timeout=2
                                    )
                            else:
                                # Face is properly framed, check emotion
                                if avg_e_score < 0.4:
                                    print(
                                        f"Face seems okay (avg score: {avg_e_score:.2f}). Switching back to BODY framing."
                                    )
                                    tracking_state = "body"

                    # Compute final distress score from rolling averages
                    distress_score = int(max(avg_e_score, avg_p_score) * 100)

                    # Send scores back to ESP32
                    update_url = f"http://{ESP32_IP}/update_scores?distress={distress_score}&emotion={e_label}&posture={p_label}"
                    requests.get(update_url, timeout=5)

                    latest_result = {
                        "image": base64.b64encode(image_bytes).decode("utf-8"),
                        "emotion": emotion_result,
                        "posture": posture_result,
                    }
                finally:
                    os.unlink(temp_file.name)

        except Exception as e:
            print(f"Background task error: {e}")

        time.sleep(0.5)


# Start background thread
threading.Thread(target=background_task, daemon=True).start()


@app.route("/status", methods=["GET"])
def status():
    return {
        "status": "ok",
        "service": "NoDistress API",
        "message": "Server is running",
    }, 200


@app.route("/latest", methods=["GET"])
def get_latest():
    global latest_result

    if latest_result["image"] is None:
        return (
            jsonify(
                {"status": "no_data", "message": "No image has been processed yet"}
            ),
            200,
        )

    return (
        jsonify(
            {
                "status": "ok",
                "image": latest_result["image"],  # base64 string
                "emotion": latest_result["emotion"],
                "posture": latest_result["posture"],
            }
        ),
        200,
    )


@app.route("/analyze", methods=["POST"])
def analyze():
    global latest_result

    # ESP32 sends raw JPEG bytes
    if not request.data:
        return jsonify({"error": "No image data received"}), 400

    image_bytes = request.data

    temp_file = tempfile.NamedTemporaryFile(delete=False, suffix=".jpg")
    temp_file.write(request.data)
    temp_file.close()

    try:
        emotion_result = get_emotion_distress_score(temp_file.name)
        posture_result = get_posture_stats(temp_file.name)

        latest_result = {
            "image": base64.b64encode(image_bytes).decode("utf-8"),
            "emotion": emotion_result,
            "posture": posture_result,
        }

        return jsonify(latest_result)

    finally:
        os.unlink(temp_file.name)


if __name__ == "__main__":
    app.run("0.0.0.0", port=3000)

"""Routes: 
POST route that takes in image data from the robot.
- Will calculate posture and emotion score of the image.
- Will return scores in this route and also hold the most recent values in memory.

GET route will return the most recent image and posture, emotion score of most recent image."""

"""
Currently saving the files to disk to run emotion and posture result.
- May not be sustainable for heavier loads, should consider migrating to memory.
"""
