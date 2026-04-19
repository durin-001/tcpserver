#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <ctime>
//定义端口号
#define PORT 8080

int main(){
	//1. 创建监听套接字
	int lfd = socket(AF_INET,SOCK_STREAM,0);
	if(lfd == -1){
		std::cerr<<"socket create failed！错误原因："<< strerror(errno) <<std::endl;
		return -1;
	}
	std::cout<< "监听套接字创建成功！lfd："<< lfd <<std::endl;

	//2. 绑定ip和端口--将监听套接字与公网的ip，端口关联
	//定义sockaddr_in结构体
	struct sockaddr_in server_addr{};
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(PORT);
	server_addr.sin_addr.s_addr = INADDR_ANY;

	int bind_ret = bind(lfd,(struct sockaddr*)&server_addr,sizeof(server_addr));
	if(bind_ret == -1){
		std::cerr<<"bind failed！错误原因: "<< strerror(errno) <<std::endl;
		close(lfd);
		return -1;
	}
	std::cout<<"IP和端口绑定成功，绑定端口："<< PORT <<std::endl;
	
	//3. 监听连接 -- 让监听套接字进入监听状态
	int listen_ret = listen(lfd,128);
	if(listen_ret == -1){
		std::cerr<<"listen failed！错误原因："<< strerror(errno) <<std::endl;
		close(lfd);
		return -1;
	}
	std::cout<<"服务器开始监听，等待客户端连接...(端口："<< PORT <<")" << std::endl;
	std::cout<<std::endl;
	
	//4. 循环接收客户端连接--死循环让服务常驻内存
	while(1){
		//接收客户端连接，返回通信套接字client fd--用于与客户端收发数据
		std::cerr << "阻塞在accept系统调用（tcp服务器程序正在等待新的客户端连接）,lfd: " << lfd <<std::endl;
		int cfd = accept(lfd,nullptr,nullptr);
		if(cfd == -1){
			std::cerr<< "accept failed! 错误原因："<< strerror(errno) << std::endl;
			continue;
		}
		std::cout<< "客户端连接成功！cfd：" << cfd << std::endl;
		while(1){
		//5. 与客户端收发数据
		char buf[1024] = {0};
		//read系统读取客户端发送的数据，返回其长度
		ssize_t read_len = read(cfd,buf,sizeof(buf) - 1);
		if(read_len < 0){
			std::cerr<< "read failed！错误原因：" << strerror(errno) <<std::endl;
			close(cfd);
			break;
		}else if(read_len == 0){
			//读取到0表示客户端主动断开连接
			std::cerr<< "客户端主动断开连接！cfd："<< cfd <<std::endl;
			std::cout<<std::endl;
			close(cfd);
			break;
		}
		std::cout<<std::endl;

		std::cout<<"收到客户端数据（cfd："<< cfd << ")："<< buf <<std::endl;
		//向客户端回复数据
		const char* resp = "server received,hello,i am proud of you as you do it perferly.\n";
		ssize_t write_len = write(cfd,resp,strlen(resp));
		if(write_len < 0){
			std::cerr <<" write failed！错误原因："<< strerror(errno) << std::endl;
			close(cfd);
			break;
		}
					
		time_t now = time(nullptr);
		char time_str[100];
		strftime(time_str,sizeof(time_str),"%Y-%m-%d- %H:%M:%S",localtime(&now));
		std::cout<<"连接处理完成时间："<< time_str << "(cfd："<< cfd <<")"<<std::endl;
	}
		//6. 单次连接处理完成，关闭当前通信套接字和客户端连接
		close(cfd);
		std::cout<< "客户端连接处理已完成，内循环无限沟通已结束，关闭当前连接回到外循环等待下一个连接" <<std::endl;
	}
	//7. 关闭监听套接字 --理论上不会执行到这里
	close(lfd);
	return 0;
}
