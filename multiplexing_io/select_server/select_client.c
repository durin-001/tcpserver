#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#define PORT 8888
#define BUF_SIZE 1024
#define SERVER_IP "127.0.0.1"

int main() {
    int client_fd;
    struct sockaddr_in server_addr;
    char buf[BUF_SIZE];
    char msg[BUF_SIZE];
    ssize_t send_len, recv_len;

    // 1. 创建客户端socket
    client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_fd == -1) {
        perror("socket create failed");
        exit(EXIT_FAILURE);
    }

    // 2. 连接服务端
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    
    // 优化：使用更现代、更安全的 inet_pton 代替 inet_addr
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        perror("inet_pton failed or invalid IP address");
        close(client_fd);
        exit(EXIT_FAILURE);
    }

    if (connect(client_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("connect failed");
        close(client_fd);
        exit(EXIT_FAILURE);
    }
    printf("已连接服务端，可发送数据（输入'quit'退出）\n");

    // 3. 发送数据并接收响应
    while (1) {
        printf("请输入要发送的内容：");
        // 如果读取失败(如遇到 EOF)直接退出
        if (fgets(msg, BUF_SIZE, stdin) == NULL) {
            printf("\n读取输入结束，退出客户端。\n");
            break;
        }

        msg[strcspn(msg, "\n")] = '\0';

        if (strcmp(msg, "quit") == 0) {
            break;
        }

        // 优化：防止发送空字符串（用户直接敲回车）
        if (strlen(msg) == 0) {
            continue; 
        }

        send_len = send(client_fd, msg, strlen(msg), 0);
        if (send_len < 0) {
            perror("send failed");
            break;
        }

        memset(buf, 0, BUF_SIZE);
        recv_len = recv(client_fd, buf, BUF_SIZE - 1, 0);
        if (recv_len < 0) {
            perror("recv failed");
            break;
        } else if (recv_len == 0) {
            printf("服务端主动断开连接\n");
            break;
        }
        printf("服务端响应：%s\n", buf);
    }

    // 4. 清理资源
    close(client_fd);
    return 0;
}