import cv2
import numpy as np
import json

# 打开视频文件
cap = cv2.VideoCapture("Seeed1.mp4")
#cap = cv2.VideoCapture(0)
assert cap.isOpened(), "Error reading video file"

# 读取视频的第一帧
ret, frame = cap.read()
if not ret:
    print("Error reading frame")
    exit()

all_region_points = []


def select_points(event, x, y, flags, param):
    global region_points
    if event == cv2.EVENT_LBUTTONDOWN:
        region_points.append([x, y])
        cv2.circle(param, (x, y), 5, (0, 0, 255), -1)
        cv2.imshow('Image', param)


while True:
    frame_copy = frame.copy()
    region_points = []
    cv2.imshow('Image', frame_copy)
    cv2.setMouseCallback('Image', select_points, frame_copy)

    while True:
        key = cv2.waitKey(0)
        if key == 13:  # 回车键
            cv2.destroyAllWindows()
            all_region_points.append(region_points)
            break

    more_boxes = input("Do you want to add another box? (y/n): ")
    if more_boxes.lower() != 'y':
        break

# 保存所有区域坐标到JSON文件
with open('regions.json', 'w') as f:
    json.dump(all_region_points, f)

print("区域坐标已保存到 regions.json 文件中")
cap.release()
