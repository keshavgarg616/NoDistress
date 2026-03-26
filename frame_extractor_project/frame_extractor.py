import cv2
import os

def extract_frames(video_path, output_folder, frames_per_second=5):
    if not os.path.exists(output_folder):
        os.makedirs(output_folder)

    cap = cv2.VideoCapture(video_path)

    if not cap.isOpened():
        print("Error: Cannot open video file")
        return

    fps = cap.get(cv2.CAP_PROP_FPS)
    frame_interval = max(1, int(fps / frames_per_second))

    frame_count = 0
    saved_count = 0

    while True:
        ret, frame = cap.read()
        if not ret:
            break

        if frame_count % frame_interval == 0:
            filename = os.path.join(output_folder, f"frame_{saved_count:05d}.jpg")
            cv2.imwrite(filename, frame)
            saved_count += 1

        frame_count += 1

    cap.release()
    print(f"Done. Saved {saved_count} frames to '{output_folder}'")


if __name__ == "__main__":
    video_path = input("Enter video path: ")
    output_folder = "output_frames"

    extract_frames(video_path, output_folder, frames_per_second=5)
