import socket

TCP_IP = '127.0.0.1'  # 本地IP地址
TCP_PORT = 5005       # 监听端口
BUFFER_SIZE = 1024    # 接收数据缓冲区大小

server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
server_socket.bind((TCP_IP, TCP_PORT))
server_socket.listen(1)

print("TCP server is waiting for connection...")

conn, addr = server_socket.accept()
print(f"Connection address: {addr}")

while True:
    data = conn.recv(BUFFER_SIZE)
    if not data:
        break
    print(f"Received data: {data.decode('utf-8')}")

conn.close()
