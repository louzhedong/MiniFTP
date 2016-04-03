// SFTPServer.cpp : 定义控制台应用程序的入口点。
#include "stdafx.h"

#include "define.h"
#include <stdio.h>
#include <winsock2.h>

//线程数据结构，含SOCKET连接套接字和客户端地址：
struct threadData {
	SOCKET	tcps;
	sockaddr_in clientaddress;
};

//FTP初始化:
int InitFTPSocket(SOCKET *pListenSock)
{
	WORD wVersionRequested;
	WSADATA wsaData;
	int ret;
	SOCKET sListen;	//侦听套接字
	SOCKADDR_IN tcpaddr;	//地址信息

	//WinSock初始化
	wVersionRequested = MAKEWORD(2, 2);			//希望使用的WinSock DLL的版本
	ret = WSAStartup( wVersionRequested, &wsaData );
	if (ret!=0)
	{
		printf("WSAStartup() failed!\n");
		return 0;
	}
	//确认WinSock DLL支持版本2.2
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE( wsaData.wVersion ) != 2 )
	{
		WSACleanup();
		printf("Invalid Winsock version!\n");
		return 0;
	}

	//创建socket，使用TCP协议：
	sListen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(sListen == INVALID_SOCKET)
	{
		WSACleanup();
		printf("socket() failed!\n");
		return 0;
	}
	//构建本地地址信息：
	tcpaddr.sin_family=AF_INET;		//地址家族
	tcpaddr.sin_port=htons(CMD_PORT);	//转化为网络字节序
	tcpaddr.sin_addr.S_un.S_addr=htonl(INADDR_ANY);		//使用INADDR_ANY	指示任意地址
	//bind:
	ret=bind(sListen,(SOCKADDR*)&tcpaddr,sizeof(tcpaddr));

	//listen:
	ret=listen(sListen, 8);

	*pListenSock = sListen;
	return 1;
}

//建立数据连接
int InitDataSocket(SOCKET *Datatcps, SOCKADDR_IN *ClientAddr)
{
	SOCKET datatcps;
	SOCKADDR_IN tcpaddress;

	//创建socket
	datatcps = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	
	memcpy(&tcpaddress, ClientAddr, sizeof(SOCKADDR_IN));
	tcpaddress.sin_port = htons(DATA_PORT);//只需修改端口值

	//请求连接客户端
	connect(datatcps, (SOCKADDR*)&tcpaddress, sizeof(tcpaddress));

	*Datatcps = datatcps;
	return 1;
}


//接收命令报文
int ReceiveCmd(SOCKET tcps, char* pCmd)
//从客户端接收信息
{
	int nRet;
	int nleft = sizeof(CmdPacket);
	
	//从控制连接中读取数据，大小为sizeof(CmdPacket)：
	while(nleft)
	{
		nRet = recv(tcps, pCmd, nleft, 0);
		if(!nRet)
		{
			printf("Connection is closed by client!\n");
			return 0;
		}
		
		nleft -= nRet;
		pCmd += nRet;
	}
	
	return 1; //成功获取命令报文
}


//发送回复报文：
int SendRspons(SOCKET tcps, RsponsPacket* prspns)
{
	send(tcps, (char *)prspns, sizeof(RsponsPacket), 0);
	return 1;
}


//发送一项文件信息：
int SendFileRecord(SOCKET datatcps, WIN32_FIND_DATA* pfd)
//used to send response to client
{
	char filerecord[MAX_PATH+32];
	FILETIME ft;
	FileTimeToLocalFileTime(&pfd->ftLastWriteTime, &ft);
	SYSTEMTIME lastwtime;
	FileTimeToSystemTime(&ft, &lastwtime);
	char* dir = pfd->dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY? "<DIR>" : "";
	sprintf(filerecord,"%04d-%02d-%02d %02d:%02d    %5s  %10d  %-20s\n",
		lastwtime.wYear,
		lastwtime.wMonth,
		lastwtime.wDay,
		lastwtime.wHour,
		lastwtime.wMinute,
		dir,
		pfd->nFileSizeLow,
		pfd->cFileName);
	send(datatcps, filerecord, strlen(filerecord), 0);
	return 1;
}

//发送文件列表信息
int SendFileList(SOCKET datatcps)
{
	HANDLE hff;
	WIN32_FIND_DATA fd;

	//搜索文件
	hff = FindFirstFile("*", &fd);
	if(hff == INVALID_HANDLE_VALUE) //发生错误
	{
		const char* errstr="Can't list files!\n";
		printf("List file error!\n");
		if(send(datatcps, errstr, strlen(errstr), 0) == SOCKET_ERROR)
		{
			printf("Error occurs when sending file list!\n");
		}
		closesocket(datatcps);
		return 0;
	}

	BOOL fMoreFiles = TRUE;
	while(fMoreFiles)
	{
		//发送此项文件信息：
		if(!SendFileRecord(datatcps, &fd))
		{
			closesocket(datatcps);
			return 0;
		}
		//搜索下一个文件：
		fMoreFiles = FindNextFile(hff, &fd);
	}

	closesocket(datatcps);
	return 1;
}

//通过数据连接发送文件：
int SendFile(SOCKET datatcps, FILE* file)
{
	char buf[1024];
	printf("文件传输中……………………………………");
	//从文件中循环读取数据并发往客户端
	for(;;)
	{
		int r = fread(buf, 1, 1024, file);
		send(datatcps, buf, r, 0);
		if(r<1024) //文件传送结束
			break;
	}
	closesocket(datatcps);
	printf("传输完毕\n");
	return 1;
}

//接收文件
int ReceiveFile(SOCKET datatcps, char* filename)
{
	char buf[1024];
	FILE* file=fopen(filename,"wb");

	//循环接收所有数据，并同时写往文件：
	printf("接收文件信息…………………………");
	for(;;)
	{
		int r = recv(datatcps, buf, 1024, 0);
		if(r == SOCKET_ERROR)
		{
			printf("从客户端接收文件出现错误！\n");
			fclose(file);
			closesocket(datatcps);
			return 0;
		}
		if(!r) //数据传送结束
			break;

		fwrite(buf, 1, r, file);
	}
	fclose(file);
	closesocket(datatcps);
	printf("传输完毕\n");
	return 1;
}

//检测文件是否存在：
int FileExists(const char *filename)
{
	WIN32_FIND_DATA fd;
	if(FindFirstFile(filename, &fd) == INVALID_HANDLE_VALUE)
		return 0;
	return 1;
}

//处理命令报文
int ProcessCmd(SOCKET tcps, CmdPacket* pCmd, SOCKADDR_IN *pClientAddr)
{
	SOCKET datatcps; //数据连接套接字
	RsponsPacket rspons;//回复报文
	FILE* file;

	//根据命令类型分派执行：
	switch(pCmd->cmdtype)
	{
	case LS: 
		//建立数据连接：
		InitDataSocket(&datatcps, pClientAddr);
		//发送文件列表信息：
		SendFileList(datatcps);
		break;
	case CD:
		//设置当前目录
		if(SetCurrentDirectory(pCmd->param))
		{
			rspons.rspnostype = OK;
			GetCurrentDirectory(RSPONS_TEXT_SIZE, rspons.text);
			SendRspons(tcps, &rspons); //发送回复报文
		}
		break;

	case PWD:
		rspons.rspnostype = OK;
		//获取当前目录，并回复
		GetCurrentDirectory(RSPONS_TEXT_SIZE, rspons.text);
		SendRspons(tcps, &rspons);
		break;
	//上传文件
	case PUT: 
		//首先发送回复报文
		char filename[64];
		strcpy(filename, pCmd->param);
		//verify no file with same name exits to make sure that no file will be overwritten
		if(FileExists(filename))
		{
			rspons.rspnostype=ERRORS;
			sprintf(rspons.text, "%s 文件已存在!\n", filename);
			SendRspons(tcps, &rspons);
		}
		else
		{
			rspons.rspnostype=OK;
			if(!SendRspons(tcps, &rspons)) 
				return 0;
			//另建一个数据连接来接收数据：
			if(!InitDataSocket(&datatcps, pClientAddr)) 
				return 0;
			if(!ReceiveFile(datatcps, filename)) 
				return 0;
		}
		break;

	//下载文件：
	case GET: 
		file=fopen(pCmd->param, "rb"); //打开要下载的文件
		if(file)
		{
			rspons.rspnostype = OK;
			sprintf(rspons.text, "下载文件 %s\n", pCmd->param);
			if(!SendRspons(tcps, &rspons))
			{
				fclose(file);
				return 0;
			}
			else
			{
				//创建额外的数据连接来传送数据：
				if(!InitDataSocket(&datatcps, pClientAddr)) 
				{
					fclose(file);
					return 0;
				}
				SendFile(datatcps, file);
				fclose(file);
			}
		}
		break;
	
	case QUIT:
		printf("退出客户端！\n");
		rspons.rspnostype = OK;
		strcpy(rspons.text, "感谢使用SFTP客户端!\n");
		SendRspons(tcps, &rspons);
		return 0;
	}
	return 1;
}







//线程函数，参数包括相应控制连接的套接字：
DWORD WINAPI ThreadFunc( LPVOID lpParam ) 
{ 
	SOCKET tcps;
	sockaddr_in clientaddr;
	
	tcps = ((threadData *)lpParam)->tcps;
	clientaddr = ((threadData *)lpParam)->clientaddress;

	printf("欢迎使用SFTP，当前连接的socket id 是 %u.\n", tcps);

	//发送回复报文给客户端，内含命令使用说明：
	printf("客户端地址 %s:%d\n", inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port));
	RsponsPacket rspns= {OK,
		"欢迎使用STFP客户端，请按照下列命令操作!\n"
		"Avilable commands:\n"
		"1.  ls\t<no param>\n"
		"2.  pwd\t<no param>\n"
		"3.  cd\t<path>\n"
		"4.  get\t<file>\n"
		"5.  put\t<file>\n"
		"6.  quit<no param>\n"
	};
	SendRspons(tcps, &rspns);

	//循环获取客户端命令报文并进行处理
	for(;;)
	{
		CmdPacket cmd;
		if(!ReceiveCmd(tcps, (char *)&cmd)) 
			break;
		if(!ProcessCmd(tcps, &cmd, &clientaddr)) 
			break;
	}
	
	//线程结束前关闭控制连接套接字：
	closesocket(tcps);
	delete lpParam;
	return 0; 
} 

int main(int argc, char* argv[])
{
	SOCKET tcps_listen;//FTP服务器控制连接侦听套接字
	struct threadData *pThInfo;

	if(!InitFTPSocket(&tcps_listen)) //FTP初始化
		return 0;
	
	printf("Mini FTP Server listening on %d port...\n", CMD_PORT);
	
	//循环接受客户端连接请求，并生成线程去处理：
	for(;;)
	{
		pThInfo = NULL;
		pThInfo = new threadData;
		if(pThInfo == NULL)
		{
			printf(" malloc space failed!\n");
			continue;
		}
		
		int len = sizeof(struct threadData);
		//等待接受客户端控制连接请求
		pThInfo->tcps = accept(tcps_listen, (SOCKADDR*)&pThInfo->clientaddress, &len);

		//创建一个线程来处理相应客户端的请求：
		DWORD dwThreadId, dwThrdParam = 1; 
		HANDLE hThread; 
		
		hThread = CreateThread( 
			NULL,                        // no security attributes 
			0,                           // use default stack size  
			ThreadFunc,                  // thread function 
			pThInfo,					// argument to thread function 
			0,                           // use default creation flags 
			&dwThreadId);                // returns the thread identifier 
		
		// Check the return value for success. 
		
		if (hThread == NULL) 
		{
			printf("CreateThread failed.\n"); 
			closesocket(pThInfo->tcps);
			delete pThInfo;
		}
	}
	return 0;
}