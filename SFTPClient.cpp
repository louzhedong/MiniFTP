// SFTPClient.cpp : �������̨Ӧ�ó������ڵ㡣
//

#include "stdafx.h"
#include <stdio.h>
#include <winsock2.h>
#include <windows.h>

#include "define.h"

//��ȡ�ظ����ģ�
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

//��������ģ�
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

//�������������׽��ֲ���������״̬��
SOCKET create_data_socket()
{
	SOCKET sockfd;
	struct sockaddr_in my_addr;
	
	// ���������������ӵ��׽��֣�
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
	
	// �󶨣�
	if(bind(sockfd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr)) == SOCKET_ERROR)
	{
		int err = WSAGetLastError();
		printf("bind error %d in create_data_socket!\n", err);
		closesocket(sockfd);
		WSACleanup();
		exit(1);
	}
	
	// ����������������
	if(listen(sockfd, 1) == SOCKET_ERROR)
	{
		printf("listen error in create_data_socket");
		closesocket(sockfd);
		WSACleanup();
		exit(1);
	}

	return sockfd;
}

//����list���
void list(SOCKET sockfd)
{
	int sin_size;
	int nRead;
	CmdPacket cmd_packet;
	SOCKET newsockfd, data_sockfd;
	struct sockaddr_in their_addr;
	char data_buf[DATA_BUFSIZE];

	//�����������ӣ�
	newsockfd = create_data_socket();
	
	//��������Ĳ���������������
	cmd_packet.cmdtype = LS;//û�в���
	do_write_cmd(sockfd, &cmd_packet);
	
	sin_size = sizeof(struct sockaddr_in);
	// ���ܷ�������������������
	if((data_sockfd = accept(newsockfd, (struct sockaddr *)&their_addr, &sin_size)) == INVALID_SOCKET )
	{
		printf("accept error in get_file");
		closesocket(newsockfd);
		closesocket(sockfd);
		WSACleanup();
		exit(1);
	}
	
	//ÿ�ζ����������ݾ���ʾ���٣�ֱ���������ӶϿ�
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
		
		if(nRead == 0) //���ݶ�ȡ����
			break;
		
		//��ʾ���ݣ�
		data_buf[nRead] = '\0';
		printf("%s", data_buf);
	}

	closesocket(data_sockfd);
	closesocket(newsockfd);
}

//����pwd���
void pwd(int sockfd)
{
	CmdPacket cmd_packet;
	RsponsPacket rspns_packet;

	cmd_packet.cmdtype = PWD;

	//��������Ĳ���ȡ�ظ���
	do_write_cmd(sockfd, &cmd_packet);
	do_read_rspns(sockfd, &rspns_packet);
	printf("%s\n", rspns_packet.text);
}

//����cd���
void cd(int sockfd)
{
	CmdPacket cmd_packet;
	RsponsPacket rspns_packet;
	
	cmd_packet.cmdtype = CD;
	scanf("%s", cmd_packet.param);

	//��������Ĳ���ȡ�ظ���
	do_write_cmd(sockfd, &cmd_packet);
	do_read_rspns(sockfd, &rspns_packet);
	if(rspns_packet.rspnostype == ERRORS)
		printf("%s", rspns_packet.text);
}

//����get����������ļ���
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
	
	//��������ģ�
	cmd_packet.cmdtype = GET;
	scanf("%s", cmd_packet.param);
	
	//�򿪻��ߴ��������ļ��Թ�д���ݣ�
	fd = fopen(cmd_packet.param, "wb");//ʹ�ö����Ʒ�ʽ
	if(fd == NULL)
	{
		printf("Open file %s for write failed!\n", cmd_packet.param);
		return;
	}
	
	//�����������Ӳ���������������������
	newsockfd = create_data_socket();
	
	//��������ģ�
	do_write_cmd(sockfd, &cmd_packet);
	
	//��ȡ�ظ����ģ�
	do_read_rspns(sockfd, &rspns_packet);
	if(rspns_packet.rspnostype == ERRORS)
	{
		printf("%s", rspns_packet.text);
		closesocket(newsockfd);
		
		fclose(fd);
		//ɾ���ļ���
		DeleteFile(cmd_packet.param);
		return;
	}
	
	sin_size = sizeof(struct sockaddr_in);
	// �ȴ����ܷ���������������
	if((data_sockfd = accept(newsockfd, (struct sockaddr *)&their_addr, &sin_size)) == INVALID_SOCKET)
	{
		printf("accept error in get_file!\n");
		closesocket(newsockfd);
		
		fclose(fd);
		//ɾ���ļ���
		DeleteFile(cmd_packet.param);
		return;
	}
	
	// ѭ����ȡ�������ݲ�д���ļ���
	while((count = recv(data_sockfd, data_buf, DATA_BUFSIZE, 0)) > 0)
		fwrite(data_buf, sizeof(char), count, fd);
	
	closesocket(data_sockfd);
	closesocket(newsockfd);
	fclose(fd);
}

//����put������ϴ��ļ���
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
	
	//�򿪱����ļ����ڶ����ݣ�
	fd = fopen(cmd_packet.param, "rb");
	if(fd == NULL)
	{
		printf("open %s for reading failed!\n", cmd_packet.param);
		return;
	}
	
	//�������������׽��ֲ���������״̬��
	newsockfd = create_data_socket();
	
	//��������ģ�
	do_write_cmd(sockfd, &cmd_packet);
	
	//��ȡ�ظ����ģ�
	do_read_rspns(sockfd, &rspns_packet);
	if(rspns_packet.rspnostype == ERRORS)
	{
		printf("%s", rspns_packet.text);
		closesocket(newsockfd);
		fclose(fd);
		return;
	}
	
	sin_size = sizeof(struct sockaddr_in);
	// ׼������������������
	if((data_sockfd = accept(newsockfd, (struct sockaddr *)&their_addr, &sin_size)) == INVALID_SOCKET)
	{
		printf("accept error in put_file!\n");
		closesocket(newsockfd);
		
		fclose(fd);
		return;
	}
	
	// ѭ�����ļ��ж�ȡ���ݲ�������������
	while(true)
	{
		count = fread(data_buf, sizeof(char), DATA_BUFSIZE, fd);
		send(data_sockfd, data_buf, count, 0);
		if(count < DATA_BUFSIZE) //�����Ѿ�������߷�������
			break;
	}
	
	closesocket(data_sockfd);
	closesocket(newsockfd);
	fclose(fd);
}

//�����˳����
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
	//Winsock��ʼ����
	err = WSAStartup( wVersionRequested, &wsaData );
	if ( err != 0 ) 
	{
		printf("WinSock initialized failed!\n");
		return;
	}

	//ȷ��WinSock DLL�İ汾��2.2��
	if(LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
	{
		printf("WinSock version is not 2.2!\n");
		WSACleanup();
		return; 
	}
	
	//�������ڿ������ӵ�socket��
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
	
	// ���ӷ�������
	if(connect(sockfd, (struct sockaddr *)&their_addr, sizeof(struct sockaddr)) == SOCKET_ERROR)
	{
		printf("connect error");
		closesocket(sockfd);
		WSACleanup();
		exit(1);
	}

	//���ӳɹ������Ƚ��շ��������ص���Ϣ��
	do_read_rspns(sockfd, &rspns_packet);
	printf("%s", rspns_packet.text);
	
	// ��ѭ������ȡ�û����벢����ִ��
	while(true)
	{
		scanf("%s", cmd);
		switch(cmd[0])
		{
			case 'l' : //����list����
				list(sockfd); 
				break;
			case 'p' : 
				if(cmd[1] == 'w') //����pwd����
					pwd(sockfd); 
				else //����put����
				   	put_file(sockfd);
				break;
			case 'c' : //����cd����
				cd(sockfd); 
				break;
			case 'g' : //����get����
				get_file(sockfd); 
				break;
			case 'q' : //����quit����
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

