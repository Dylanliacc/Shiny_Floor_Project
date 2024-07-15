#include <iostream>
#include <string.h>
#include <unistd.h>
#include <wait.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <sys/select.h>

int ai_fd = 0, other_fd = 0;
int board[20], board_num = 0;
char last_buffer[30] = {0};

int init_server(int &fd, int nPort, int nNum, int nOpt) {
    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("[server] - socket");
        return -1;
    }

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(nPort);

    if (nOpt) {
        if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &nOpt, sizeof(nOpt)) < 0) {
            perror("[server] - setsockopt");
            return -1;
        }
    }

    if (bind(fd, (struct sockaddr *) &address, sizeof(address)) < 0) {
        perror("[server] - bind");
        return -1;
    }

    if (listen(fd, nNum) < 0) {
        perror("[server] - listen");
        return -1;
    }

    return 0;
}

int accept_client(const int &server_fd) {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    int client_fd = accept(server_fd, (struct sockaddr *) &client_addr, &client_len);
    if (client_fd < 0) {
        perror("[server] - accept");
        exit(1);
    }

    std::cout << "\n[server] - id: " << client_fd << "\n";
    std::cout << "[server] - Addr: " << inet_ntoa(client_addr.sin_addr) << "\n";
    std::cout << "[server] - port: " << client_addr.sin_port << "\n\n";

    if (strcmp(inet_ntoa(client_addr.sin_addr), "127.0.0.1") == 0) {
        ai_fd = client_fd;
        printf("ai client connect now [%d]\n", ai_fd);
    } else {
        board_num++;
    }

    return client_fd;
}

void distribute(const char *value, int num) {
    for (int i = 0; i < num; ++i) {
        if (board[i + 1] == 0) {
            continue;
        }
	if (value[i] == last_buffer[i] ) {
	    continue;
	}

        if (value[i] == '0') {
            write(board[i + 1], "off", 3);
        } else {
            write(board[i + 1], "rainbow", 7);
        }

        printf("%d -> %d: %c\n", i + 1, board[i + 1], value[i]);
    }
}

void identify(int fd, const char *value, int num) {
    switch (value[0]) {
        case '1':
            board[1] = fd;
            printf("board 1 is %d\n", fd);
            break;
        case '2':
            board[2] = fd;
            printf("board 2 is %d\n", fd);
            break;
        case '3':
            board[3] = fd;
            printf("board 3 is %d\n", fd);
            break;
        case '4':
            board[4] = fd;
            printf("board 4 is %d\n", fd);
            break;
    }
}

int main() {
    int server_fd, max_fd = 0;
// const int nPort = 5005;
    const int nPort = 3333;
    const int nNum = 15;

    if (-1 == init_server(server_fd, nPort, nNum, 1)) {
        printf("[server] - init_server failed\n");
        return 1;
    }

    fd_set read_fds, tmp_fds;
    struct timeval time_val = {
        .tv_sec = 0,
        .tv_usec = 300 * 1000,
    };

    FD_ZERO(&read_fds);
    FD_SET(server_fd, &read_fds);
    max_fd = std::max(max_fd, server_fd);

    while (true) {
        FD_ZERO(&tmp_fds);
        tmp_fds = read_fds;

        int num = select(max_fd + 1, &tmp_fds, NULL, NULL, &time_val);
        if (num == 0) {
            // printf("[sever] - select timeout\n");
            continue;
        } else if (num == -1) {
            perror("select");
            break;
        }

        for (int i = 0; i <= max_fd && num; ++i) {
            if (!FD_ISSET(i, &tmp_fds)) {
                continue;
            }

            --num;
            if (i == server_fd) {
                int client_fd = accept_client(server_fd);

                max_fd = std::max(max_fd, client_fd);
                FD_SET(client_fd, &read_fds);
            } else {
                char buffer[30] = {0};
                int ret = read(i, buffer, sizeof(buffer));
                if (ret) {
                    std::cout << "[server] - recv: " << buffer << "\n";
                    if (i == ai_fd) {
                        distribute(buffer, ret);
			memcpy(last_buffer, buffer, ret);
                    } else {
                        identify(i, buffer, ret);
                    }
                } else {
                    close(i);
                    FD_CLR(i, &read_fds);
                    std::cout << "[server] - remove client " << i << "\n";
                }
            }
        }
    }

    close(server_fd);

    return 0;
}
