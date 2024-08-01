import sys
import gi
gi.require_version('Gst', '1.0')
from gi.repository import Gst, GLib
import cv2
import numpy as np
import json
from ultralytics import YOLO
import socket

# 初始化GStreamer
Gst.init(None)

# 初始化YOLO模型
model = YOLO("yolov8n-pose.engine")

# 打开两个视频文件
pipeline1 = Gst.parse_launch(
    "v4l2src device=/dev/video0 ! "
    "video/x-raw, format=(string)YUY2, width=640, height=480, framerate=30/1 ! "
    "videoconvert ! video/x-raw, format=(string)RGB ! appsink name=sink1 max-buffers=1 drop=true"
)

pipeline2 = Gst.parse_launch(
    "v4l2src device=/dev/video2 ! "
    "video/x-raw, format=(string)YUY2, width=640, height=480, framerate=30/1 ! "
    "videoconvert ! video/x-raw, format=(string)RGB ! appsink name=sink2 max-buffers=1 drop=true"
)

sink1 = pipeline1.get_by_name('sink1')
sink1.set_property("emit-signals", True)

sink2 = pipeline2.get_by_name('sink2')
sink2.set_property("emit-signals", True)

def new_sample(sink, data):
    sample = sink.emit("pull-sample")
    if not sample:
        print("Error: No sample received from GStreamer pipeline.")
        return None

    buf = sample.get_buffer()
    if not buf:
        print("Error: No buffer received from sample.")
        return None

    caps = sample.get_caps()
    # 获取宽度、高度和通道数
    width = caps.get_structure(0).get_value("width")
    height = caps.get_structure(0).get_value("height")
    channels = 3  # 假设图像为RGB格式

    # 确保缓冲区大小正确
    buffer_size = buf.get_size()
    expected_size = width * height * channels
    if buffer_size != expected_size:
        print(f"Error: Buffer size {buffer_size} does not match expected size {expected_size}")
        return None

    # 将缓冲区转换为numpy数组
    arr = np.ndarray(
        (height, width, channels),
        buffer=buf.extract_dup(0, buffer_size),
        dtype=np.uint8
    )
    return arr

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

# 主循环
def bus_call(bus, message, loop):
    if message.type == Gst.MessageType.EOS:
        loop.quit()
    elif message.type == Gst.MessageType.ERROR:
        err, debug = message.parse_error()
        print(f"Error: {err}, {debug}")
        loop.quit()
    return True

def main():
    loop = GLib.MainLoop()

    bus1 = pipeline1.get_bus()
    bus1.add_signal_watch()
    bus1.connect("message", bus_call, loop)

    bus2 = pipeline2.get_bus()
    bus2.add_signal_watch()
    bus2.connect("message", bus_call, loop)

    pipeline1.set_state(Gst.State.PLAYING)
    pipeline2.set_state(Gst.State.PLAYING)

    try:
        while True:
            sample1 = new_sample(sink1, None)
            sample2 = new_sample(sink2, None)
            regions_status = [0] * (len(all_region_points1) + len(all_region_points2))

            if sample1 is not None:
                image1 = cv2.cvtColor(sample1, cv2.COLOR_RGB2BGR)
                
                # 进行目标跟踪
                results1 = model.track(image1, verbose=False, show=False)
                mesh_data1 = results1[0].keypoints.data

                # 绘制关键点和区域
                image1 = plot_keypoint(mesh_data1, image1, all_region_points1, regions_status, 0)
                results1[0].plot()
                cv2.imshow("Camera 1", image1)
            
            if sample2 is not None:
                image2 = cv2.cvtColor(sample2, cv2.COLOR_RGB2BGR)
                
                # 进行目标跟踪
                results2 = model.track(image2, verbose=False, show=False)
                mesh_data2 = results2[0].keypoints.data

                # 绘制关键点和区域
                image2 = plot_keypoint(mesh_data2, image2, all_region_points2, regions_status, len(all_region_points1))
                results2[0].plot()
                cv2.imshow("Camera 2", image2)
            
            status_string = ''.join(map(str, regions_status))
            print(f"Regions status: {status_string}")

            # 通过TCP发送状态字符串
            binary_data = status_string.encode('utf-8')
            client_socket.send(binary_data)
            
            cv2.waitKey(1)
            
    except KeyboardInterrupt:
        pass

    pipeline1.set_state(Gst.State.NULL)
    pipeline2.set_state(Gst.State.NULL)

    cv2.destroyAllWindows()

if __name__ == '__main__':
    main()
