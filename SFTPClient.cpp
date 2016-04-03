// SFTPClient.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include <stdio.h>
#include <winsock2.h>
#include <windows.h>

#include "define.h"

//读取回复报文：
void do_read_rspns(SOCKET fd, RsponsPacket *ptr)
{
	int count = 0;
	int size = sizeof(RsponsPacket);
	while(count < size)
	{
		int nRead = recv(fd, (char *)ptr+count, size-count, 0);
		if(nRead <= 0)
		{
			printf("read response error");
			closesocket(fd);
			exit(1);
		}
		count += nRead;
	}
}

//发送命令报文：
void do_write_cmd(SOCKET fd, CmdPacket *ptr)
{
	int size = sizeof(CmdPacket);

	int flag = send(fd, (char *)ptr, size, 0);

	if(flag == SOCKET_ERROR)
	{
		printf("write command error");
		closesocket(fd);
		WSACleanup();
		exit(1);
	}
}

//创建数据连接套接字并进入侦听状态：
SOCKET create_data_socket()
{
	SOCKET sockfd;
	struct sockaddr_in my_addr;
	
	// 创建用于数据连接的套接字：
	if((sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET )
	{
		printf("socket error in create_data_socket");
		WSACleanup();
		exit(1);
	}
	
	my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons(DATA_PORT);
	my_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	memset(&(my_addr.sin_zero), 0, sizeof(my_addr.sin_zero));
	
	// 绑定：
	if(bind(sockfd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr)) == SOCKET_ERROR)
	{
		int err = WSAGetLastError();
		printf("bind error %d in create_data_socket!\n", err);
		closesocket(sockfd);
		WSACleanup();
		exit(1);
	}
	
	// 侦听数据连接请求：
	if(listen(sockfd, 1) == SOCKET_ERROR)
	{
		printf("listen error in create_data_socket");
		closesocket(sockfd);
		WSACleanup();
		exit(1);
	}

	return sockfd;
}

//处理list命令：
void list(SOCKET sockfd)
{
	int sin_size;
	int nRead;
	CmdPacket cmd_packet;
	SOCKET newsockfd, data_sockfd;
	struct sockaddr_in their_addr;
	char data_buf[DATA_BUFSIZE];

	//创建数据连接：
	newsockfd = create_data_socket();
	
	//构建命令报文并发送至服务器：
	cmd_packet.cmdtype = LS;//没有参数
	do_write_cmd(sockfd, &cmd_packet);
	
	sin_size = sizeof(struct sockaddr_in);
	// 接受服务器的数据连接请求：
	if((data_sockfd = accept(newsockfd, (struct sockaddr *)&their_addr, &sin_size)) == INVALID_SOCKET )
	{
		printf("accept error in get_file");
		closesocket(newsockfd);
		closesocket(sockfd);
		WSACleanup();
		exit(1);
	}
	
	//每次读到多少数据就显示多少，直到数据连接断开
	while(true)
	{
		nRead = recv(data_sockfd, data_buf, DATA_BUFSIZE-1, 0);
		if(nRead == SOCKET_ERROR)
		{
			printf("Read response error!\n");
			closesocket(data_sockfd);
			closesocket(newsockfd);
			closesocket(sockfd);
			WSACleanup();
			exit(1);
		}
		
		if(nRead == 0) //数据读取结束
			break;
		
		//显示数据：
		data_buf[nRead] = '\0';
		printf("%s", data_buf);
	}

	closesocket(data_sockfd);
	closesocket(newsockfd);
}

//处理pwd命令：
void pwd(int sockfd)
{
	CmdPacket cmd_packet;
	RsponsPacket rspns_packet;

	cmd_packet.cmdtype = PWD;

	//发送命令报文并读取回复：
	do_write_cmd(sockfd, &cmd_packet);
	do_read_rspns(sockfd, &rspns_packet);
	printf("%s\n", rspns_packet.text);
}

//处理cd命令：
void cd(int sockfd)
{
	CmdPacket cmd_packet;
	RsponsPacket rspns_packet;
	
	cmd_packet.cmdtype = CD;
	scanf("%s", cmd_packet.param);

	//发送命令报文并读取回复：
	do_write_cmd(sockfd, &cmd_packet);
	do_read_rspns(sockfd, &rspns_packet);
	if(rspns_packet.rspnostype == ERRORS)
		printf("%s", rspns_packet.text);
}

//处理get命令，即下载文件：
void get_file(SOCKET sockfd)
{
	FILE *fd;
	char data_buf[DATA_BUFSIZE];

	CmdPacket cmd_packet;
	RsponsPacket rspns_packet;
	
	SOCKET newsockfd, data_sockfd;
	struct sockaddr_in their_addr;
	int sin_size;
	int count;
	
	//设置命令报文：
	cmd_packet.cmdtype = GET;
	scanf("%s", cmd_packet.param);
	
	//打开或者创建本地文件以供写数据：
	fd = fopen(cmd_packet.param, "wb");//使用二进制方式
	if(fd == NULL)
	{
		printf("Open file %s for write failed!\n", cmd_packet.param);
		return;
	}
	
	//创建数据连接并侦听服务器的连接请求：
	newsockfd = create_data_socket();
	
	//发送命令报文：
	do_write_cmd(sockfd, &cmd_packet);
	
	//读取回复报文：
	do_read_rspns(sockfd, &rspns_packet);
	if(rspns_packet.rspnostype == ERRORS)
	{
		printf("%s", rspns_packet.text);
		closesocket(newsockfd);
		
		fclose(fd);
		//删除文件：
		DeleteFile(cmd_packet.param);
		return;
	}
	
	sin_size = sizeof(struct sockaddr_in);
	// 等待接受服务器的连接请求
	if((data_sockfd = accept(newsockfd, (struct sockaddr *)&their_addr, &sin_size)) == INVALID_SOCKET)
	{
		printf("accept error in get_file!\n");
		closesocket(newsockfd);
		
		fclose(fd);
		//删除文件：
		DeleteFile(cmd_packet.param);
		return;
	}
	
	// 循环读取网络数据并写入文件：
	while((count = recv(data_sockfd, data_buf, DATA_BUFSIZE, 0)) > 0)
		fwrite(data_buf, sizeof(char), count, fd);
	
	closesocket(data_sockfd);
	closesocket(newsockfd);
	fclose(fd);
}

//处理put命令，即上传文件：
void put_file(SOCKET sockfd)
{
	FILE *fd;
	CmdPacket cmd_packet;
	RsponsPacket rspns_packet;
	char data_buf[DATA_BUFSIZE];
	
	SOCKET newsockfd, data_sockfd;
	struct sockaddr_in their_addr;
	int sin_size;
	int count;
	
	cmd_packet.cmdtype = PUT;
	scanf("%s", cmd_packet.param);
	
	//打开本地文件用于读数据：
	fd = fopen(cmd_packet.param, "rb");
	if(fd == NULL)
	{
		printf("open %s for reading failed!\n", cmd_packet.param);
		return;
	}
	
	//创建数据连接套接字并进入侦听状态：
	newsockfd = create_data_socket();
	
	//发送命令报文：
	do_write_cmd(sockfd, &cmd_packet);
	
	//读取回复报文：
	do_read_rspns(sockfd, &rspns_packet);
	if(rspns_packet.rspnostype == ERRORS)
	{
		printf("%s", rspns_packet.text);
		closesocket(newsockfd);
		fclose(fd);
		return;
	}
	
	sin_size = sizeof(struct sockaddr_in);
	// 准备接受数据连接请求：
	if((data_sockfd = accept(newsockfd, (struct sockaddr *)&their_addr, &sin_size)) == INVALID_SOCKET)
	{
		printf("accept error in put_file!\n");
		closesocket(newsockfd);
		
		fclose(fd);
		return;
	}
	
	// 循环从文件中读取数据并发给服务器：
	while(true)
	{
		count = fread(data_buf, sizeof(char), DATA_BUFSIZE, fd);
		send(data_sockfd, data_buf, count, 0);
		if(count < DATA_BUFSIZE) //数据已经读完或者发生错误
			break;
	}
	
	closesocket(data_sockfd);
	closesocket(newsockfd);
	fclose(fd);
}

//处理退出命令：
void quit(int sockfd)
{
	CmdPacket cmd_packet;
	RsponsPacket rspns_packet;
	
	cmd_packet.cmdtype = QUIT;
	do_write_cmd(sockfd, &cmd_packet);
	
	do_read_rspns(sockfd, &rspns_packet);
	printf("%s", rspns_packet.text);
}

void main(int argc, char *argv[])
{
	
	SOCKET sockfd;
	struct hostent *he;
	struct sockaddr_in their_addr;
	char cmd[10];
	RsponsPacket rspns_packet;
		
	WORD wVersionRequested;
	WSADATA wsaData;
	int err;
	
	if(argc != 2)
	{
		printf("usage : SFTPClient hostname(or IP address)\n");
		exit(1);
	}

	wVersionRequested = MAKEWORD( 2, 2 );
	//Winsock初始化：
	err = WSAStartup( wVersionRequested, &wsaData );
	if ( err != 0 ) 
	{
		printf("WinSock initialized failed!\n");
		return;
	}

	//确认WinSock DLL的版本是2.2：
	if(LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
	{
		printf("WinSock version is not 2.2!\n");
		WSACleanup();
		return; 
	}
	
	//创建用于控制连接的socket：
	sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(sockfd == INVALID_SOCKET)
	{
		printf("Failed to create socket!\n");
		WSACleanup();
		exit(1);
	}

	if((he = gethostbyname(argv[1])) == NULL)
	{
		printf("gethostbyname failed!");
		exit(1);
	}

	their_addr.sin_family = AF_INET;
	their_addr.sin_port = htons(CMD_PORT);
	their_addr.sin_addr = *(struct in_addr *)he->h_addr_list[0];
	memset(&(their_addr.sin_zero), 0, sizeof(their_addr.sin_zero));
	
	// 连接服务器：
	if(connect(sockfd, (struct sockaddr *)&their_addr, sizeof(struct sockaddr)) == SOCKET_ERROR)
	{
		printf("connect error");
		closesocket(sockfd);
		WSACleanup();
		exit(1);
	}

	//连接成功后，首先接收服务器发回的消息：
	do_read_rspns(sockfd, &rspns_packet);
	printf("%s", rspns_packet.text);
	
	// 主循环：读取用户输入并分派执行
	while(true)
	{
		scanf("%s", cmd);
		switch(cmd[0])
		{
			case 'l' : //处理list命令
				list(sockfd); 
				break;
			case 'p' : 
				if(cmd[1] == 'w') //处理pwd命令
					pwd(sockfd); 
				else //处理put命令
				   	put_file(sockfd);
				break;
			case 'c' : //处理cd命令
				cd(sockfd); 
				break;
			case 'g' : //处理get命令
				get_file(sockfd); 
				break;
			case 'q' : //处理quit命令
				quit(sockfd); 
				break;
			default : 
				printf("invalid command!\n");
				break;
		}
		if(cmd[0] == 'q')
			break;
	}
	
	WSACleanup();
}

