#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>

#define PORT 8888
#define BUF_SIZE 1024

//设置 socket 为非阻塞模式
void set_nonblocking(int fd) {
    //获取文件描述符标志
    int flags = fcntl(fd, F_GETFL, 0);
    if(flags == -1) {
        perror("fcntl F_GETFL failed！");
        return;
    }
    //设置非阻塞状态
    int fcntl_ret = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    if(fcntl_ret == -1) {
        perror("fcntl F_SETFL failed！");
    }
}

//设置指定毫秒数
void msleep(int ms) {
    if(ms < 0) return;
    struct timespec ts;
    ts.tv_sec = ms / 1000;   //tv_sec:time value seconds,取输入毫秒数的秒部分
    ts.tv_nsec = (ms % 1000) * 1000000;   //取输入毫秒数的余数部分-纳秒
    //休眠函数--参数以纳秒为单位
    nanosleep(&ts, NULL);
}

int main() {
    int client_fd;
    struct sockaddr_in server_addr;
    char buf[BUF_SIZE] = {0};
    char msg[BUF_SIZE];
    ssize_t send_len, recv_len, read_len;
    int connect_ret;
    int error;
    socklen_t error_len = sizeof(error);

    //1. 创建客户端socket，设置为非阻塞模式
    client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(client_fd == -1) {
        perror("socket create failed！");
        exit(EXIT_FAILURE);
    }
    //套接字设置为非阻塞
    set_nonblocking(client_fd);
    //把键盘输入也设置为非阻塞，防止卡住收发循环
    set_nonblocking(STDIN_FILENO);
    //2. 非阻塞连接
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_addr.sin_port = htons(PORT);

    printf("正在连接服务器...");
    connect_ret = connect(client_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    if(connect_ret == -1 && errno != EINPROGRESS) {
        perror("connect failed!");
        close(client_fd);
        exit(EXIT_FAILURE);
    }

    //轮询等待连接完成--非阻塞连接需要手动检查连接状态
    int connected = 0;
    while(!connected) {
        connect_ret = connect(client_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));
        if (connect_ret == 0 || errno == EISCONN)
        {
            connected = 0;
            printf("已经连接到服务器(输入quit可退出)...\n");
        }else if(connect_ret == EALREADY || connect_ret == EINPROGRESS) {
            //还在连接中，休息一会再连接
            msleep(50);
        }else{
            printf("连接失败...");
            close(client_fd);
            exit(EXIT_FAILURE);
        }
    }

    //3. 发送数据
    while(1) {
        //  A：从键盘读取输入
        printf("请输入要发送的内容：\n");
        //文件输入函数
        //opt1：字符数组指针，用于存储读取的数据；opt2：最大读取字符数（包括结尾的\0）；opt3：文件指针
        //成功返回mag指针，失败返回NULL
        //fgets(msg, BUF_SIZE, stdin);           //其缓冲机制对非阻塞的数据收发有影响
        memset(msg, 0, BUF_SIZE);
        read_len = read(STDERR_FILENO, msg, BUF_SIZE - 1);
        if (read_len > 0)
        {
            msg[strcmp(msg, "\n")] = '\0';   //查找并替换msg中的指定字符
        //退出功能
            if(strcspn(msg, "quit") == 0) {
                printf("客户端主动退出!");
                break;
            }
            if (strlen(msg) > 0)
            {
                send_len = send(client_fd, msg, strlen(msg), 0);
                if(send_len < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                    printf("send failed!");
                    break;
                }
                printf("successfully send %d bytes to nonblock_server!" ,strlen(msg));
            }
        }else if (send_len < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
        {
            perror("read stdin failed!");
            break;
        }
        
        //非阻塞接收服务器响应
        memset(buf, 0, BUF_SIZE);
        recv_len = recv(client_fd, buf, BUF_SIZE - 1, 0);
        if(recv_len > 0) {
            printf("服务器响应:%s\n", buf);
        }else if(recv_len == 0) {
            printf("服务器断开连接\n");
            break;
        }else if(read_len < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                perror("recv failed!");
                break;
        }
        //C：休眠，避免轮询占满CPU
        msleep(50);
    }
    //关闭连接
    close(client_fd);
    return 0;
}
