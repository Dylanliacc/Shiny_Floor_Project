import cv2
import numpy as np
import json
from ultralytics import YOLO

# 初始化YOLO模型
model = YOLO("yolov8s-pose.pt")

# 打开视频文件
cap = cv2.VideoCapture(0)
assert cap.isOpened(), "Error reading video file"

# 读取JSON文件中的区域坐标
with open('regions.json', 'r') as f:
    all_region_points = json.load(f)

keypoint_check = 16

# 创建视频写入对象
w, h, fps = (int(cap.get(x)) for x in (cv2.CAP_PROP_FRAME_WIDTH, cv2.CAP_PROP_FRAME_HEIGHT, cv2.CAP_PROP_FPS))
video_writer = cv2.VideoWriter("workouts2.avi", cv2.VideoWriter_fourcc(*'mp4v'), fps, (w, h))


def plot_keypoint(keypoint_list, image):
    regions_status = [0] * len(all_region_points)

    for ind, k in enumerate(reversed(keypoint_list)):
        if len(k) != 0:
            k = k.cpu().numpy()
            cv2.circle(image, (int(k[keypoint_check][0]), int(k[keypoint_check][1])), 3, (0, 255, 0), 2,
                       lineType=cv2.LINE_AA)

            for idx, region_points in enumerate(all_region_points):
                region_points = np.array(region_points, np.int32)
                cv2.polylines(image, [region_points], True, (0, 255, 255), thickness=4)
                if cv2.pointPolygonTest(region_points, (k[keypoint_check][0], k[keypoint_check][1]),
                                        False) >= 0 or cv2.pointPolygonTest(region_points, (
                k[keypoint_check - 1][0], k[keypoint_check - 1][1]), False) >= 0:
                    cv2.polylines(image, [region_points], True, (0, 255, 0), thickness=4)
                    regions_status[idx] = 1
    status_string = ''.join(map(str, regions_status))
    print(f"Regions status: {status_string}")
    cv2.putText(image, status_string,(100,100),5,5,(0,0,255),13)

    cv2.imshow("test", image)
    cv2.waitKey(1)
    video_writer.write(image)


# 主循环
while cap.isOpened():
    success, image = cap.read()
    if not success:
        print("Video frame is empty or video processing has been successfully completed.")
        break

    # 进行目标跟踪
    results = model.track(image, verbose=False, show=False)
    mesh_data = results[0].keypoints.data

    # 绘制关键点和区域
    plot_keypoint(mesh_data, image)
    results[0].plot()

# 释放资源
cv2.destroyAllWindows()
video_writer.release()
cap.release()
