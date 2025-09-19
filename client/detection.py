import cv2
import mediapipe as mp
import numpy as np
import sys

# Initialize MediaPipe Hands
mp_hands = mp.solutions.hands
hands = mp_hands.Hands(
    max_num_hands=1,
    min_detection_confidence=0.7,
    min_tracking_confidence=0.5
)
mp_draw = mp.solutions.drawing_utils

def detect_hand_gesture():
    cap = cv2.VideoCapture(0) # Use default camera

    if not cap.isOpened():
        print("Error: Could not open video stream.", file=sys.stderr)
        return "ERROR: Camera not found"

    # Give the camera a moment to warm up and get a stable frame
    for _ in range(10): # Read a few frames
        ret, frame = cap.read()
        if not ret:
            print("Error: Could not read frame from camera.", file=sys.stderr)
            cap.release()
            return "ERROR: Could not read frame"

    # Read a single frame for detection
    ret, frame = cap.read()
    if not ret:
        print("Error: Could not read frame from camera for detection.", file=sys.stderr)
        cap.release()
        return "ERROR: Could not read frame"

    # Flip the frame horizontally for a mirror effect
    frame = cv2.flip(frame, 1)
    # Convert the BGR image to RGB.
    rgb_frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)

    # Process the frame and find hands
    result = hands.process(rgb_frame)

    gesture = "Nothing" # Default gesture

    if result.multi_hand_landmarks:
        for hand_landmarks in result.multi_hand_landmarks:
            # Get landmark coordinates
            landmarks = []
            for lm in hand_landmarks.landmark:
                landmarks.append([lm.x, lm.y, lm.z])

            # Simple gesture recognition logic
            # TODO: improve this? to another method?
            
            # Example: Rock (all fingers curled)
            # Paper (all fingers extended)
            # Scissors (index and middle extended, others curled)

            # Thumb (TIP: 4, IP: 3, MCP: 2, CMC: 1)
            # Index (TIP: 8, IP: 7, MCP: 6, PIP: 5)
            # Middle (TIP: 12, IP: 11, MCP: 10, PIP: 9)
            # Ring (TIP: 16, IP: 15, MCP: 14, PIP: 13)
            # Pinky (TIP: 20, IP: 19, MCP: 18, PIP: 17)

            # Check if fingers are extended (y-coordinate of tip is higher than PIP/MCP)
            # For simplicity, let's use a basic check based on y-coordinates
            # (Note: This is a very basic heuristic and might not be robust)

            # Thumb: tip (4) is higher than IP (3)
            thumb_extended = landmarks[4][1] < landmarks[3][1]

            # Index: tip (8) is higher than PIP (6)
            index_extended = landmarks[8][1] < landmarks[6][1]

            # Middle: tip (12) is higher than PIP (10)
            middle_extended = landmarks[12][1] < landmarks[10][1]

            # Ring: tip (16) is higher than PIP (14)
            ring_extended = landmarks[16][1] < landmarks[14][1]

            # Pinky: tip (20) is higher than PIP (18)
            pinky_extended = landmarks[20][1] < landmarks[18][1]

            # Rock: All fingers curled (not extended)
            if not index_extended and not middle_extended and not ring_extended and not pinky_extended:
                gesture = "Rock"
            # Paper: All fingers extended
            elif index_extended and middle_extended and ring_extended and pinky_extended:
                gesture = "Paper"
            # Scissors: Index and Middle extended, others curled
            elif index_extended and middle_extended and not ring_extended and not pinky_extended:
                gesture = "Scissors"
            else:
                gesture = "Unknown" # Fallback for other hand shapes

            # For simplicity, we'll just take the first hand detected
            # NOTE: this program won't capture multiple hands. why do I even need that feature?
            break

    cap.release()
    return gesture

if __name__ == "__main__":
    detected_gesture = detect_hand_gesture()
    print(detected_gesture)
