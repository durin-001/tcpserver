#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#define PORT 8888
#define BUF_SIZE 1024

int main(){
	//c语言采用 “声明和定义分离”的设计哲学
	int client_fd;
	struct sockaddr_in server_addr;
	char buf[BUF_SIZE] = {0};
	char msg[BUF_SIZE];
	ssize_t send_len,recv_len;

	//1. 创建客户端socket
	client_fd = socket(AF_INET,SOCK_STREAM,0);
	if(client_fd == -1){
		perror("socket create failed！");
		exit(EXIT_FAILURE);
	}

	//2. 连接服务器--connect阻塞进程，直到连接
	memset(&server_addr,0,sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");  //绑定服务器IP--本地回环
	server_addr.sin_port = htons(PORT);

	int connect_ret = connect(client_fd,(struct sockaddr*)&server_addr,sizeof(server_addr));
	if(connect_ret == -1){
		perror("connect failed");
		close(client_fd);
		exit(EXIT_FAILURE);
	}
	printf("服务器连接成功！可发送数据（输入‘quit'退出）\n");

	//3. 发送数据
	while(1){
		printf("请输入要发送的内容：");
		fgets(msg,BUF_SIZE,stdin);
		msg[strcspn(msg,"\n")] = '\0';   //去除fgets读取到的换行符
		
		//退出
		if(strcmp(msg,"quit") == 0){
			break;
		}

		//发送数据
		send_len = send(client_fd,msg,strlen(msg),0);
		if(send_len < 0){
			perror("send failed!");
			break;
		}
		printf("数据发送成功！%s",msg);
		//阻塞接受服务器响应
		memset(buf,0,BUF_SIZE);
		recv_len = recv(client_fd,buf,BUF_SIZE - 1,0);
		if(recv_len < 0){
			perror("recv failed");
			break;
		}else if(recv_len == 0){
			printf("服务器断开连接");
			break;
		}
		printf("服务器响应：%s\n",buf);
	}
	close(client_fd);
	return 0;

}
