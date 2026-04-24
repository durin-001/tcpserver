#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#define PORT 8888
#define BUF_SIZE 2048

int main(){
	int server_fd,client_fd;
	struct sockaddr_in server_addr,client_addr;
	socklen_t client_addr_len = sizeof(client_addr);
	char buf[BUF_SIZE] = {0};
	ssize_t recv_len,send_len;

	//1. 创建TCP socket--默认是阻塞模式
	server_fd = socket(AF_INET,SOCK_STREAM,0);
	if(server_fd == -1) {
		perror("Socket create failed！");
		exit(EXIT_FAILURE);
	}

	//2. 绑定IP和端口
	memset(&server_addr,0,sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htons(INADDR_ANY);
	server_addr.sin_port = htons(PORT);

	int bind_res = bind(server_fd,(struct sockaddr*)&server_addr,sizeof(server_addr));
	if(bind_res == -1){
		perror("Bind failed！");
		close(server_fd);
		exit(EXIT_FAILURE);
	}
	
	//3.监听--backlog == 5即最多允许5个等待连接
	int listen_res = listen(server_fd, 5);
	if(listen_res == -1){
		perror("Listen failed！");
		close(server_fd);
		exit(EXIT_FAILURE);
	}
	printf("开始监听！阻塞IO服务器端已启动，等待客户端连接...\n");

	//4. 阻塞等待客户端连接--accept是阻塞等待
	client_fd = accept(server_fd,(struct sockaddr*)&client_addr,&client_addr_len);
	if(client_fd == -1){
		perror("accpet failed！");
		close(server_fd);
		exit(EXIT_FAILURE);
	}
	printf("客户端(IP:%s，端口：%d)已经连接\n",inet_ntoa(client_addr.sin_addr),ntohs(client_addr.sin_port));

	//5. 阻塞接受客户端数据--recv是阻塞接收
	while(1){
		memset(buf, 0, BUF_SIZE);   //清空接收缓冲区
		recv_len = recv(client_fd, buf, BUF_SIZE - 1, 0);
		if(recv_len < 0){
			perror("recv failed!");
			break;
		}else if(recv_len == 0){
			printf("客户端主动断开连接");
			break;
		}
		//打印收到的数据
		printf("收到客户端数据：%s\n",buf);

		//6. [ send阻塞进程送数据--直到数据发送完成 ]
		char response[BUF_SIZE];
		//这里是自动回复：已经收到：buf你发送的内容
		snprintf(response, sizeof(response), "自动回复，已经收到：%s", buf);
		
		int send_res = send(client_fd, response, strlen(response), 0);
		if(send_res < 0){
			perror("Send failed！");
			break;
		}
		printf("此服务器不能主动做出手动回复\n");
	}
	//关闭连接--必须要先关闭客户端
	close(client_fd);
	close(server_fd);
	return 0;
}
