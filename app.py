import base64
import tempfile

from flask import Flask, jsonify, request
from CV.ExpressionDetector.emotion_detection import get_emotion_distress_score

app = Flask(__name__)

#Holding latest result in memory for get requests to frontend

latest_result = {
    "image": None,      # base64 encoded image
    "emotion": None,
    "posture": None
}

@app.route("/", methods=["GET"])
def status():
    return {
        "status": "ok",
        "service": "NoDistress API",
        "message": "Server is running"
    }, 200

@app.route("/analyze", methods=["POST"])
def analyze():
    global latest_result

    # ESP32 sends raw JPEG bytes
    if not request.data:
        return jsonify({"error": "No image data received"}), 400
    
    image_bytes = request.data


    # save raw bytes to temp file
    temp_file = tempfile.NamedTemporaryFile(delete=False, suffix=".jpg")
    temp_file.write(request.data)
    temp_file.close()

    try:
        emotion_result = get_emotion_distress_score(temp_file.name)

        latest_result = {
            "image": base64.b64encode(image_bytes).decode("utf-8"),
            "emotion": emotion_result,
            "posture": None  # placeholder for now
        }

        return jsonify(latest_result)

    finally:
        pass


if __name__ == "__main__":
    app.run("0.0.0.0", port=3000)

'''Routes: 
POST route that takes in image data from the robot.
- Will calculate posture and emotion score of the image.
- Will return scores in this route and also hold the most recent values in memory.

GET route will return the most recent image and posture, emotion score of most recent image.'''