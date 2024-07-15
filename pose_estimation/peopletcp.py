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

# 打开视频文件
pipeline = Gst.parse_launch(
    "v4l2src device=/dev/video0 ! "
    "video/x-raw, format=(string)YUY2, width=640, height=480, framerate=30/1 ! "
    "videoconvert ! video/x-raw, format=(string)RGB ! appsink name=sink max-buffers=1 drop=true"
)
# 打开视频文件
sink = pipeline.get_by_name('sink')
sink.set_property("emit-signals", True)


def new_sample(sink, data):
    sample = sink.emit("pull-sample")
    buf = sample.get_buffer()
    caps = sample.get_caps()
    # 获取宽度、高度和通道数
    width = caps.get_structure(0).get_value("width")
    height = caps.get_structure(0).get_value("height")
    channels = 3  # 假设图像为RGB格式

    # 确保缓冲区大小正确
    buffer_size = buf.get_size()
    expected_size = width * height * channels
    if buffer_size != expected_size:
        raise ValueError(f"Buffer size {buffer_size} does not match expected size {expected_size}")

    # 将缓冲区转换为numpy数组
    arr = np.ndarray(
        (height, width, channels),
        buffer=buf.extract_dup(0, buffer_size),
        dtype=np.uint8
    )
    return arr

# 读取JSON文件中的区域坐标
with open('regions.json', 'r') as f:
    all_region_points = json.load(f)

keypoint_check = 16  #16



# 设置TCP客户端
TCP_IP = '127.0.0.1'  # TCP服务器的IP地址
# TCP_PORT = 5005  # TCP服务器的端口
TCP_PORT = 3333  # TCP服务器的端口
BUFFER_SIZE = 1024  # 接收数据缓冲区大小

client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
client_socket.connect((TCP_IP, TCP_PORT))


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

    # 通过TCP发送状态字符串
    binary_data = status_string.encode('utf-8')
    client_socket.send(binary_data)

    cv2.imshow("test", image)
    cv2.waitKey(1)

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

    bus = pipeline.get_bus()
    bus.add_signal_watch()
    bus.connect("message", bus_call, loop)

    pipeline.set_state(Gst.State.PLAYING)
    try:
        while True:
            sample = new_sample(sink, None)
            if sample is not None:
                image = cv2.cvtColor(sample, cv2.COLOR_RGB2BGR)
                
                # 进行目标跟踪
                results = model.track(image, verbose=True, show=False)
                mesh_data = results[0].keypoints.data

                # 绘制关键点和区域
                plot_keypoint(mesh_data, image)
                results[0].plot()
    except KeyboardInterrupt:
        pass

    pipeline.set_state(Gst.State.NULL)

    cv2.destroyAllWindows()

if __name__ == '__main__':
    main()
