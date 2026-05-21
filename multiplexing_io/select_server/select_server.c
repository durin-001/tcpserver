#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/select.h>
#include <string.h>
#include <signal.h>

#define PORT 8080
#define BUF_SIZE 1024
#define MAX_CLIENT 100

#if MAX_CLIENT > FD_SETSIZE
#error "MAX_CLIENT CANN'T OVER FD_SETSIZE"
#endif

//忽略 SIGPIPE，防止 send 到已经关闭的连接导致进程退出
static void ignore_sigpipe() {
    signal(SIGPIPE, SIG_IGN);
}

// 设置socket为非阻塞模式
int set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
    {
        perror("fcntl F_GETFL failed!");
        close(fd);
        return -1;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
    {
        perror("fcntl F_SETFL failed!");
        close(fd);
        return -1;
    }
    return 0;
}

int main()
{
    int server_fd = -1, client_fd = -1;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    char buf[BUF_SIZE] = {0};
    ssize_t send_len, recv_len;

    fd_set read_fds; // 文件描述符集合
    int max_fd = 0;
    // 引入数组记录当前在线的客户端数量
    int client_fds[MAX_CLIENT];

    ignore_sigpipe();

    for (int i = 0; i < MAX_CLIENT; i++)
    {
        client_fds[i] = -1;
    }

    // 1. 创建socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1)
    {
        perror("socket create failed!");
        exit(EXIT_FAILURE);
    }
    set_nonblocking(server_fd);
    max_fd = server_fd;

    // 2. 绑定IP和端口
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(PORT);

    // 端口复用
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    int bind_ret = bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (bind_ret == -1)
    {
        perror("bind failed!");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // 3. 监听连接
    int listen_ret = listen(server_fd, 5);
    if (listen_ret == -1)
    {
        perror("listen failed!");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    printf("select版TCP服务器已开始监听...\n");

    while (1)
    {
        // 重新初始化文件描述符集合
        FD_ZERO(&read_fds);           // 清空集合
        FD_SET(server_fd, &read_fds); // 将服务器socket加入监听--用于接收新连接

        // 将真正的有效客户端fd加入监听集合
        for (int i = 0; i < MAX_CLIENT; i++)
        {
            if (client_fds[i] != -1)
            {
                FD_SET(client_fds[i], &read_fds);
            }
        }
        // 设置select超时时间为1s,避免无限阻塞，便于定期清理和重置max_fd
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        // select:监听读事件
        int select_ret = select(max_fd + 1, &read_fds, NULL, NULL, &tv);
        if (select_ret < 0)
        {
            if (errno == EINTR) // 被硬中断打断
            {
                continue;
            }
            perror("select failed!");
            break;
        }
        else if (select_ret == 0)
        {
            continue; // 超时:1s内没有FD就绪，跳过
        }

        // 监听socket就绪--处理新客户端的连接
        if (FD_ISSET(server_fd, &read_fds)) // 测试server_fd是否在集合中（就绪）
        {
            while (1)
            {
                client_addr_len = sizeof(client_addr);
                // 接收新连接，获取客户端的fd和地址信息
                client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);

                if (client_fd < 0)
                {
                    // 非阻塞下在线程IO时会暂时的返回错误码，hulue
                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                    {
                        break;
                    }
                    if (errno == EINTR)
                    {
                        continue;          //信号中断
                    }
                    perror("accept failed!");
                    break;
                }
                // 为新客户端的套接字设置为非阻塞
                if (set_nonblocking(client_fd) < 0)
                {
                    close(client_fd);
                    continue;
                }
                
                // 将新的fd放入管理数组的空闲槽位
                int added = 0;
                for (int i = 0; i < MAX_CLIENT; i++)
                {
                    if (client_fds[i] == -1)
                    {
                        client_fds[i] = client_fd;
                        added = 1;
                        break;
                    }
                }
                if (added)
                {
                    // 更新最大文件描述符
                    if (client_fd > max_fd)
                    {
                        max_fd = client_fd;
                    }
                    printf("新客户端连接成功 (fd=%d, ip=%s, port=%d)\n",
                           client_fd,
                           inet_ntoa(client_addr.sin_addr),
                           ntohs(client_addr.sin_port));
                }
                else
                {
                    printf("已达最大连接数，拒绝连接...\n");
                    close(client_fd);
                }
            }
        }

        //-----处理已有客户端的数据收发
        for (int i = 0; i < MAX_CLIENT; i++)
        {
            int fd = client_fds[i];
            if (fd == -1)
            {
                continue;
            }

            // 检查该客户端fd是否在 read_fds 中就绪
            if (FD_ISSET(fd, &read_fds))
            {
                memset(buf, 0, BUF_SIZE);
                recv_len = recv(fd, buf, BUF_SIZE - 1, 0); // 接收数据，留一个字符给'\0'

                if (recv_len > 0)
                {
                    buf[recv_len] = '\0';
                    printf("收到客户端(fd:%d)数据:%s\n", fd, buf);

                    // 构造响应消息--回声服务器，将收到的消息原封不动返回
                    char response[BUF_SIZE + 64];
                    snprintf(response, sizeof(response), "已收到:%s\n", buf);

                    ssize_t total = 0;
                    ssize_t len = strlen(response);
                    while (total < len)
                    {
                        // 发送响应
                        send_len = send(fd, response + total, len - total, 0);
                        if (send_len > 0)
                        {
                            total += send_len;
                        }
                        else if (errno == EAGAIN || errno == EWOULDBLOCK)
                        {
                            fprintf(stderr, "fd:%d send buffer full, data may be lost\n", fd);
                            break;
                        }else{
                            perror("send failed!");
                            close(fd);
                            client_fds[i] = -1;
                            break;
                        }
                    }
                }
                else if (recv_len == 0)
                {
                    //
                    printf("客户端(fd:%d)正常断开连接(发送了FIN)\n", fd);
                    close(fd);
                    client_fds[i] = -1;
                }
                else if (recv_len < 0)
                {
                    // 出错管理--EAGAIN/EWOULDBLOCK（无数据可读时返回的错误码）
                    if (errno != EAGAIN && errno != EWOULDBLOCK)
                    {
                        perror("recv failed!");
                        close(fd);
                        client_fds[i] = -1;
                    }
                }
            }
        }
        // 动态更新fd, 防止其无限递增或维持错误的高位数值
        max_fd = server_fd;
        for (int i = 0; i < MAX_CLIENT; i++)
        {
            if (client_fds[i] > max_fd)
            {
                max_fd = client_fds[i];
            }
        }
    }
    // 关闭所有仍在连接中的客户端socket
    for (int i = 0; i < MAX_CLIENT; i++)
    {
        if (client_fds[i] != -1)
        {
            close(client_fds[i]);
        }
    }
    // main结束，关闭服务器监听socket
    close(server_fd);
    return 0;
}