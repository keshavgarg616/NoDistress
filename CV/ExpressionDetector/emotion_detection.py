"""
emotion_detection.py
Uses DeepFace to analyze a face image and return a distress score (0.0 - 1.0)

Usage:
    from emotion_detection import get_emotion_distress_score

    result = get_emotion_distress_score("path/to/image.jpg")
    print(result)
"""

from deepface import DeepFace


# Emotions that indicate distress
BAD_EMOTIONS = ["sad", "fear", "angry", "disgust"]

# Emotions that indicate the person is okay
GOOD_EMOTIONS = ["happy", "neutral"]

# Thresholds
DISTRESS_THRESHOLD = 0.7
MILD_CONCERN_THRESHOLD = 0.4


def get_emotion_distress_score(image_path: str) -> dict:


    # --- Step 1: Run DeepFace ---
    try:
        result = DeepFace.analyze(
            img_path=image_path,
            actions=["emotion"],
            enforce_detection=False  # Don't crash if face isn't perfectly detected
        )
        emotions = result[0]["emotion"]

    except FileNotFoundError:
        return _error_result(f"Image not found: {image_path}")
    except Exception as e:
        return _error_result(f"DeepFace failed: {str(e)}")

    # --- Step 2: Check we got valid emotion data ---
    if not emotions:
        return _error_result("No emotion data returned")

    # --- Step 3: Calculate distress score ---
    bad_score = sum(emotions.get(e, 0) for e in BAD_EMOTIONS)
    good_score = sum(emotions.get(e, 0) for e in GOOD_EMOTIONS)

    # Avoid division by zero with small epsilon
    score = round(bad_score / (bad_score + good_score + 0.001), 3)

    # --- Step 4: Assign a label ---
    if score > DISTRESS_THRESHOLD:
        label = "distress"
    elif score > MILD_CONCERN_THRESHOLD:
        label = "mild_concern"
    else:
        label = "okay"

    # --- Step 5: Find the dominant emotion ---
    dominant_emotion = max(emotions, key=emotions.get)

    return {
        "score": score,
        "label": label,
        "dominant_emotion": dominant_emotion,
        "emotions": emotions,
        "error": None
    }


def _error_result(message: str) -> dict:
    """Returns a safe fallback result when something goes wrong."""
    return {
        "score": -1,
        "label": "error",
        "dominant_emotion": None,
        "emotions": {},
        "error": message
    }


# --- Quick test (run this file directly to test) ---
if __name__ == "__main__":
    import sys

    if len(sys.argv) < 2:
        print("Usage: python emotion_detection.py <image_path>")
        sys.exit(1)

    image = sys.argv[1]
    result = get_emotion_distress_score(image)

    print("\n=== Emotion Distress Result ===")
    print(f"Score:            {result['score']}")
    print(f"Label:            {result['label']}")
    print(f"Dominant Emotion: {result['dominant_emotion']}")
    print(f"All Emotions:     {result['emotions']}")
    if result['error']:
        print(f"Error:            {result['error']}")
