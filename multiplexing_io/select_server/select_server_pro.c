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

/* 忽略 SIGPIPE */
static void ignore_sigpipe()
{
    signal(SIGPIPE, SIG_IGN);
}

/* 设置非阻塞 */
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

/* 客户端状态结构 */
typedef struct
{
    int fd;
    int used;

    char recv_buf[BUF_SIZE];
    size_t recv_len;

    char send_buf[BUF_SIZE];
    size_t send_len;
} conn_t;

int main()
{
    int server_fd = -1, client_fd = -1;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len;
    ssize_t send_len, recv_len;

    fd_set read_fds, write_fds;
    int max_fd = 0;

    conn_t clients[MAX_CLIENT];

    ignore_sigpipe();

    for (int i = 0; i < MAX_CLIENT; i++)
    {
        clients[i].fd = -1;
        clients[i].used = 0;
        clients[i].recv_len = 0;
        clients[i].send_len = 0;
    }

    /* 1. 创建 socket */
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1)
    {
        perror("socket create failed!");
        exit(EXIT_FAILURE);
    }
    set_nonblocking(server_fd);
    max_fd = server_fd;

    /* 2. 绑定 */
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(PORT);

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        perror("bind failed!");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    /* 3. 监听 */
    if (listen(server_fd, 5) == -1)
    {
        perror("listen failed!");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    printf("select版TCP服务器已开始监听...\n");

    while (1)
    {
        FD_ZERO(&read_fds);
        FD_ZERO(&write_fds);

        FD_SET(server_fd, &read_fds);
        max_fd = server_fd;

        /* 设置客户端监听 */
        for (int i = 0; i < MAX_CLIENT; i++)
        {
            if (!clients[i].used)
                continue;

            FD_SET(clients[i].fd, &read_fds);
            if (clients[i].send_len > 0)
            {
                FD_SET(clients[i].fd, &write_fds);
            }

            if (clients[i].fd > max_fd)
            {
                max_fd = clients[i].fd;
            }
        }

        struct timeval tv = {1, 0};
        int select_ret = select(max_fd + 1, &read_fds, &write_fds, NULL, &tv);
        if (select_ret < 0)
        {
            if (errno == EINTR)
                continue;
            perror("select failed!");
            break;
        }
        else if (select_ret == 0)
        {
            continue;
        }

        /* 新连接 */
        if (FD_ISSET(server_fd, &read_fds))
        {
            while (1)
            {
                client_addr_len = sizeof(client_addr);
                client_fd = accept(server_fd,
                                   (struct sockaddr *)&client_addr,
                                   &client_addr_len);
                if (client_fd < 0)
                {
                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                        break;
                    if (errno == EINTR)
                        continue;
                    perror("accept failed!");
                    break;
                }

                if (set_nonblocking(client_fd) < 0)
                {
                    close(client_fd);
                    continue;
                }

                int added = 0;
                for (int i = 0; i < MAX_CLIENT; i++)
                {
                    if (!clients[i].used)
                    {
                        clients[i].fd = client_fd;
                        clients[i].used = 1;
                        clients[i].recv_len = 0;
                        clients[i].send_len = 0;
                        added = 1;
                        break;
                    }
                }

                if (added)
                {
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

        /* 处理客户端 */
        for (int i = 0; i < MAX_CLIENT; i++)
        {
            if (!clients[i].used)
                continue;

            int fd = clients[i].fd;

            /* 读事件 */
            if (FD_ISSET(fd, &read_fds))
            {
                recv_len = recv(fd,
                                clients[i].recv_buf + clients[i].recv_len,
                                BUF_SIZE - clients[i].recv_len,
                                0);
                if (recv_len > 0)
                {
                    clients[i].recv_len += recv_len;

                    /* 构造响应 */
                    size_t copy_len = clients[i].recv_len;
                    if (copy_len > BUF_SIZE - clients[i].send_len)
                        copy_len = BUF_SIZE - clients[i].send_len;

                    memcpy(clients[i].send_buf + clients[i].send_len,
                           clients[i].recv_buf,
                           copy_len);
                    clients[i].send_len += copy_len;
                    clients[i].recv_len = 0;
                }
                else if (recv_len == 0)
                {
                    printf("客户端(fd:%d)正常断开连接\n", fd);
                    close(fd);
                    clients[i].used = 0;
                }
                else if (errno != EAGAIN && errno != EWOULDBLOCK)
                {
                    perror("recv failed!");
                    close(fd);
                    clients[i].used = 0;
                }
            }

            /* 写事件 */
            if (FD_ISSET(fd, &write_fds) && clients[i].send_len > 0)
            {
                send_len = send(fd,
                                clients[i].send_buf,
                                clients[i].send_len,
                                0);
                if (send_len > 0)
                {
                    memmove(clients[i].send_buf,
                            clients[i].send_buf + send_len,
                            clients[i].send_len - send_len);
                    clients[i].send_len -= send_len;
                }
                else if (send_len < 0 &&
                         errno != EAGAIN && errno != EWOULDBLOCK)
                {
                    perror("send failed!");
                    close(fd);
                    clients[i].used = 0;
                }
            }
        }
    }

    for (int i = 0; i < MAX_CLIENT; i++)
    {
        if (clients[i].used)
        {
            close(clients[i].fd);
        }
    }
    close(server_fd);
    return 0;
}