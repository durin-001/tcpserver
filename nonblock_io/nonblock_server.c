#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUF_SIZE 4096
#define MAX_CLIENT 100

// 设置socket为非阻塞
void set_nonblocking(int fd){
    int flags = fcntl(fd, F_GETFL, 0);
    if(flags == -1){
        perror("fcntl F_GETFL failed");
        return;
    }
    if(fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1){
        perror("fcntl F_SETFL failed");
    }
}

// 毫秒级休眠
void msleep(int ms){
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000;
    nanosleep(&ts, NULL);
}

int main(){
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    char buf[BUF_SIZE] = {0};
    ssize_t recv_len, send_len;

    int client_fds[MAX_CLIENT] = {0};
    int client_count = 0;

    // 1. 创建socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(server_fd == -1){
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    set_nonblocking(server_fd);

    // 2. 绑定端口
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(PORT);

    if(bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1){
        perror("bind failed!\n");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // 3. 监听
    if(listen(server_fd, 5) == -1){
        perror("listen failed!\n");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    printf("非阻塞服务端已启动，等待客户端连接...\n");

    //循环处理每个连接
    while(1){
        // 4. 非阻塞 accept
        memset(&client_addr, 0, sizeof(client_addr));
        client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_addr_len);
        if(client_fd == -1) {
            if(errno != EAGAIN && errno != EWOULDBLOCK){
                perror("accpet error!");
            }
        }
        if(client_fd != -1){
            if(client_count < MAX_CLIENT){
                set_nonblocking(client_fd);
                client_fds[client_count++] = client_fd;
                printf("新客户端连接：IP=%s, PORT=%d，当前连接数：%d\n",
                       inet_ntoa(client_addr.sin_addr),
                       ntohs(client_addr.sin_port),
                       client_count);
            }else{
                printf("连接数已满，拒绝新连接\n");
                close(client_fd);
            }
        }

        // 5. 轮询所有客户端
        for(int i = 0; i < client_count; i++){
            int fd = client_fds[i];
            if(fd == 0) continue;

            memset(buf, 0, BUF_SIZE);
            recv_len = recv(fd, buf, BUF_SIZE - 1, 0);
            if(recv_len > 0){
                // 收到数据
                printf("DEBUG:收到 %zd 字节数据！\n", recv_len);
                buf[recv_len] = '\0';
                printf("客户端[%d]发送的数据：%s\n", fd, buf);
                
                //扩大缓冲区--防止拼接截断
                char response[BUF_SIZE + 64];
                snprintf(response, BUF_SIZE, "服务端已收到：%s", buf);
                int response_len = strlen(response);
                int send_res = send(fd, response, response_len, 0);
                if(send_res > 0) {
                    printf("send successfully,已发送到内核缓冲区!\n");
                }else{
                    printf("send failed!\n");
                }
            }
            else if(recv_len == 0){
                // 客户端断开
                printf("客户端[%d] 断开连接\n", fd);
                close(fd);
                client_fds[i] = 0;
            }
            else{
                // 读错误（非 EAGAIN 才是真错误）
                if(errno != EAGAIN && errno != EWOULDBLOCK){
                    perror("read error");
                    close(fd);
                    client_fds[i] = 0;
                }
            }
        }

        // 休眠降低CPU占用
        fflush(stdout);
        msleep(50);
    }

    close(server_fd);
    return 0;
}
