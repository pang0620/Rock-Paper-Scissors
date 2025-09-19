import cv2
import mediapipe as mp
import statistics as st
from collections import deque
import sys

def compute_fingers(hand_landmarks, count):
    # Coordinates are used to determine whether a finger is being held up or not
    if hand_landmarks[8][2] < hand_landmarks[6][2]: count += 1  # Index
    if hand_landmarks[12][2] < hand_landmarks[10][2]: count += 1  # Middle
    if hand_landmarks[16][2] < hand_landmarks[14][2]: count += 1  # Ring
    if hand_landmarks[20][2] < hand_landmarks[18][2]: count += 1  # Pinky

    # Thumb
    if hand_landmarks[4][3] == "Left" and hand_landmarks[4][1] > hand_landmarks[3][1]: count += 1
    elif hand_landmarks[4][3] == "Right" and hand_landmarks[4][1] < hand_landmarks[3][1]: count += 1
    return count

# Setup MediaPipe
mp_hands = mp.solutions.hands

# Webcam
webcam = cv2.VideoCapture(0)

# Mapping fingers to moves
display_values = ["Rock", "Invalid", "Scissors", "Invalid", "Invalid", "Paper"]
move_buffer = deque(['Nothing'] * 5, maxlen=5)
last_sent_move = "Nothing"

with mp_hands.Hands(
        model_complexity=0,
        min_detection_confidence=0.5,
        min_tracking_confidence=0.5) as hands:

    while webcam.isOpened():
        success, image = webcam.read()
        if not success:
            continue

        image = cv2.flip(image, 1)
        image_rgb = cv2.cvtColor(image, cv2.COLOR_BGR2RGB)
        results = hands.process(image_rgb)

        current_move = "Nothing"
        if results.multi_hand_landmarks:
            hand_landmarks_list = []
            hand_label = results.multi_handedness[0].classification[0].label
            
            for hand_landmarks in results.multi_hand_landmarks:
                count = 0
                landmarks = []
                for id, landmark in enumerate(hand_landmarks.landmark):
                    imgH, imgW, _ = image.shape
                    xPos, yPos = int(landmark.x * imgW), int(landmark.y * imgH)
                    landmarks.append([id, xPos, yPos, hand_label])
                
                count = compute_fingers(landmarks, 0)
                if count <= 5:
                    current_move = display_values[count]
                else:
                    current_move = "Invalid"
                break # Process only one hand

        move_buffer.appendleft(current_move)

        try:
            # Stabilize detection by finding the most common move in the buffer
            stable_move = st.mode(move_buffer)
        except st.StatisticsError:
            stable_move = "Nothing"

        # Send the move to stdout if it's a new, valid move
        if stable_move != last_sent_move and stable_move != "Invalid" and stable_move != "Nothing":
            print(stable_move)
            sys.stdout.flush() # Ensure the C++ parent process receives the message immediately
            last_sent_move = stable_move
        
        # Optional: Display the camera feed for debugging
        # cv2.imshow('Detection Feed', image)
        # if cv2.waitKey(5) & 0xFF == 27:
        #     break

webcam.release()
# cv2.destroyAllWindows()
