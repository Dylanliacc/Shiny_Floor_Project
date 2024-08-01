import cv2
import numpy as np
import json
from ultralytics import YOLO
import socket

# 初始化YOLO模型
model = YOLO("yolov8n-pose.engine")

# 打开两个本地视频文件
cap1 = cv2.VideoCapture('Seeed1.mp4')
cap2 = cv2.VideoCapture('Seeed1.mp4')

# 读取JSON文件中的区域坐标
with open('regions.json', 'r') as f:
    all_region_points1 = json.load(f)

with open('regions1.json', 'r') as f:
    all_region_points2 = json.load(f)

keypoints_check = [15, 16]  # 左右脚关键点
detection_radius = 16  # 设置检测半径

# 设置TCP客户端
TCP_IP = '127.0.0.1'  # TCP服务器的IP地址
TCP_PORT = 3333  # TCP服务器的端口
BUFFER_SIZE = 1024  # 接收数据缓冲区大小

client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
client_socket.connect((TCP_IP, TCP_PORT))

def plot_keypoint(keypoint_list, image, region_points, regions_status, index_shift=0):
    keypoint_positions = []

    for ind, k in enumerate(reversed(keypoint_list)):
        if len(k) != 0:
            k = k.cpu().numpy()
            for keypoint_check in keypoints_check:
                keypoint_positions.append((int(k[keypoint_check][0]), int(k[keypoint_check][1])))

    for idx, region_points in enumerate(region_points):
        region_points = np.array(region_points, np.int32)
        cv2.polylines(image, [region_points], True, (0, 255, 255), thickness=4)

        for pos in keypoint_positions:
            # 绘制膨胀范围的圆形区域
            cv2.circle(image, pos, detection_radius, (0, 0, 255), 2)  # 用蓝色绘制检测范围

            # 检查膨胀关键点范围是否与多边形区域重叠
            if cv2.pointPolygonTest(region_points, pos, False) >= 0 or \
               any(cv2.pointPolygonTest(region_points, (pos[0] + dx, pos[1] + dy), False) >= 0 
                   for dx in range(-detection_radius, detection_radius + 1) 
                   for dy in range(-detection_radius, detection_radius + 1) 
                   if dx*dx + dy*dy <= detection_radius*detection_radius):
                cv2.polylines(image, [region_points], True, (0, 255, 0), thickness=4)
                regions_status[idx + index_shift] = 1
                break

    return image

def main():
    while cap1.isOpened() and cap2.isOpened():
        ret1, frame1 = cap1.read()
        ret2, frame2 = cap2.read()
        regions_status = [0] * (len(all_region_points1) + len(all_region_points2))

        if not ret1 and not ret2:
            break

        if ret1:
            # 进行目标跟踪
            results1 = model.track(frame1, verbose=False, show=False)
            mesh_data1 = results1[0].keypoints.data

            # 绘制关键点和区域
            frame1 = plot_keypoint(mesh_data1, frame1, all_region_points1, regions_status, 0)
            results1[0].plot()
            cv2.imshow("Camera 1", frame1)
        
        if ret2:
            # 进行目标跟踪
            results2 = model.track(frame2, verbose=False, show=False)
            mesh_data2 = results2[0].keypoints.data

            # 绘制关键点和区域
            frame2 = plot_keypoint(mesh_data2, frame2, all_region_points2, regions_status, len(all_region_points1))
            results2[0].plot()
            cv2.imshow("Camera 2", frame2)
        
        status_string = ''.join(map(str, regions_status))
        print(f"Regions status: {status_string}")

        # 通过TCP发送状态字符串x
        binary_data = status_string.encode('utf-8')
        client_socket.send(binary_data)
        
        if cv2.waitKey(1) & 0xFF == ord('q'):
            break
    
    cap1.release()
    cap2.release()
    cv2.destroyAllWindows()

if __name__ == '__main__':
    main()
