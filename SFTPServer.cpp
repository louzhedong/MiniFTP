// SFTPServer.cpp : �������̨Ӧ�ó������ڵ㡣
#include "stdafx.h"

#include "define.h"
#include <stdio.h>
#include <winsock2.h>

//�߳����ݽṹ����SOCKET�����׽��ֺͿͻ��˵�ַ��
struct threadData {
	SOCKET	tcps;
	sockaddr_in clientaddress;
};

//FTP��ʼ��:
int InitFTPSocket(SOCKET *pListenSock)
{
	WORD wVersionRequested;
	WSADATA wsaData;
	int ret;
	SOCKET sListen;	//�����׽���
	SOCKADDR_IN tcpaddr;	//��ַ��Ϣ

	//WinSock��ʼ��
	wVersionRequested = MAKEWORD(2, 2);			//ϣ��ʹ�õ�WinSock DLL�İ汾
	ret = WSAStartup( wVersionRequested, &wsaData );
	if (ret!=0)
	{
		printf("WSAStartup() failed!\n");
		return 0;
	}
	//ȷ��WinSock DLL֧�ְ汾2.2
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE( wsaData.wVersion ) != 2 )
	{
		WSACleanup();
		printf("Invalid Winsock version!\n");
		return 0;
	}

	//����socket��ʹ��TCPЭ�飺
	sListen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(sListen == INVALID_SOCKET)
	{
		WSACleanup();
		printf("socket() failed!\n");
		return 0;
	}
	//�������ص�ַ��Ϣ��
	tcpaddr.sin_family=AF_INET;		//��ַ����
	tcpaddr.sin_port=htons(CMD_PORT);	//ת��Ϊ�����ֽ���
	tcpaddr.sin_addr.S_un.S_addr=htonl(INADDR_ANY);		//ʹ��INADDR_ANY	ָʾ�����ַ
	//bind:
	ret=bind(sListen,(SOCKADDR*)&tcpaddr,sizeof(tcpaddr));

	//listen:
	ret=listen(sListen, 8);

	*pListenSock = sListen;
	return 1;
}

//������������
int InitDataSocket(SOCKET *Datatcps, SOCKADDR_IN *ClientAddr)
{
	SOCKET datatcps;
	SOCKADDR_IN tcpaddress;

	//����socket
	datatcps = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	
	memcpy(&tcpaddress, ClientAddr, sizeof(SOCKADDR_IN));
	tcpaddress.sin_port = htons(DATA_PORT);//ֻ���޸Ķ˿�ֵ

	//�������ӿͻ���
	connect(datatcps, (SOCKADDR*)&tcpaddress, sizeof(tcpaddress));

	*Datatcps = datatcps;
	return 1;
}


//���������
int ReceiveCmd(SOCKET tcps, char* pCmd)
//�ӿͻ��˽�����Ϣ
{
	int nRet;
	int nleft = sizeof(CmdPacket);
	
	//�ӿ��������ж�ȡ���ݣ���СΪsizeof(CmdPacket)��
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
	
	return 1; //�ɹ���ȡ�����
}


//���ͻظ����ģ�
int SendRspons(SOCKET tcps, RsponsPacket* prspns)
{
	send(tcps, (char *)prspns, sizeof(RsponsPacket), 0);
	return 1;
}


//����һ���ļ���Ϣ��
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

//�����ļ��б���Ϣ
int SendFileList(SOCKET datatcps)
{
	HANDLE hff;
	WIN32_FIND_DATA fd;

	//�����ļ�
	hff = FindFirstFile("*", &fd);
	if(hff == INVALID_HANDLE_VALUE) //��������
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
		//���ʹ����ļ���Ϣ��
		if(!SendFileRecord(datatcps, &fd))
		{
			closesocket(datatcps);
			return 0;
		}
		//������һ���ļ���
		fMoreFiles = FindNextFile(hff, &fd);
	}

	closesocket(datatcps);
	return 1;
}

//ͨ���������ӷ����ļ���
int SendFile(SOCKET datatcps, FILE* file)
{
	char buf[1024];
	printf("�ļ������С���������������������������");
	//���ļ���ѭ����ȡ���ݲ������ͻ���
	for(;;)
	{
		int r = fread(buf, 1, 1024, file);
		send(datatcps, buf, r, 0);
		if(r<1024) //�ļ����ͽ���
			break;
	}
	closesocket(datatcps);
	printf("�������\n");
	return 1;
}

//�����ļ�
int ReceiveFile(SOCKET datatcps, char* filename)
{
	char buf[1024];
	FILE* file=fopen(filename,"wb");

	//ѭ�������������ݣ���ͬʱд���ļ���
	printf("�����ļ���Ϣ��������������������");
	for(;;)
	{
		int r = recv(datatcps, buf, 1024, 0);
		if(r == SOCKET_ERROR)
		{
			printf("�ӿͻ��˽����ļ����ִ���\n");
			fclose(file);
			closesocket(datatcps);
			return 0;
		}
		if(!r) //���ݴ��ͽ���
			break;

		fwrite(buf, 1, r, file);
	}
	fclose(file);
	closesocket(datatcps);
	printf("�������\n");
	return 1;
}

//����ļ��Ƿ���ڣ�
int FileExists(const char *filename)
{
	WIN32_FIND_DATA fd;
	if(FindFirstFile(filename, &fd) == INVALID_HANDLE_VALUE)
		return 0;
	return 1;
}

//���������
int ProcessCmd(SOCKET tcps, CmdPacket* pCmd, SOCKADDR_IN *pClientAddr)
{
	SOCKET datatcps; //���������׽���
	RsponsPacket rspons;//�ظ�����
	FILE* file;

	//�����������ͷ���ִ�У�
	switch(pCmd->cmdtype)
	{
	case LS: 
		//�����������ӣ�
		InitDataSocket(&datatcps, pClientAddr);
		//�����ļ��б���Ϣ��
		SendFileList(datatcps);
		break;
	case CD:
		//���õ�ǰĿ¼
		if(SetCurrentDirectory(pCmd->param))
		{
			rspons.rspnostype = OK;
			GetCurrentDirectory(RSPONS_TEXT_SIZE, rspons.text);
			SendRspons(tcps, &rspons); //���ͻظ�����
		}
		break;

	case PWD:
		rspons.rspnostype = OK;
		//��ȡ��ǰĿ¼�����ظ�
		GetCurrentDirectory(RSPONS_TEXT_SIZE, rspons.text);
		SendRspons(tcps, &rspons);
		break;
	//�ϴ��ļ�
	case PUT: 
		//���ȷ��ͻظ�����
		char filename[64];
		strcpy(filename, pCmd->param);
		//verify no file with same name exits to make sure that no file will be overwritten
		if(FileExists(filename))
		{
			rspons.rspnostype=ERRORS;
			sprintf(rspons.text, "%s �ļ��Ѵ���!\n", filename);
			SendRspons(tcps, &rspons);
		}
		else
		{
			rspons.rspnostype=OK;
			if(!SendRspons(tcps, &rspons)) 
				return 0;
			//��һ�������������������ݣ�
			if(!InitDataSocket(&datatcps, pClientAddr)) 
				return 0;
			if(!ReceiveFile(datatcps, filename)) 
				return 0;
		}
		break;

	//�����ļ���
	case GET: 
		file=fopen(pCmd->param, "rb"); //��Ҫ���ص��ļ�
		if(file)
		{
			rspons.rspnostype = OK;
			sprintf(rspons.text, "�����ļ� %s\n", pCmd->param);
			if(!SendRspons(tcps, &rspons))
			{
				fclose(file);
				return 0;
			}
			else
			{
				//��������������������������ݣ�
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
		printf("�˳��ͻ��ˣ�\n");
		rspons.rspnostype = OK;
		strcpy(rspons.text, "��лʹ��SFTP�ͻ���!\n");
		SendRspons(tcps, &rspons);
		return 0;
	}
	return 1;
}







//�̺߳���������������Ӧ�������ӵ��׽��֣�
DWORD WINAPI ThreadFunc( LPVOID lpParam ) 
{ 
	SOCKET tcps;
	sockaddr_in clientaddr;
	
	tcps = ((threadData *)lpParam)->tcps;
	clientaddr = ((threadData *)lpParam)->clientaddress;

	printf("��ӭʹ��SFTP����ǰ���ӵ�socket id �� %u.\n", tcps);

	//���ͻظ����ĸ��ͻ��ˣ��ں�����ʹ��˵����
	printf("�ͻ��˵�ַ %s:%d\n", inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port));
	RsponsPacket rspns= {OK,
		"��ӭʹ��STFP�ͻ��ˣ��밴�������������!\n"
		"Avilable commands:\n"
		"1.  ls\t<no param>\n"
		"2.  pwd\t<no param>\n"
		"3.  cd\t<path>\n"
		"4.  get\t<file>\n"
		"5.  put\t<file>\n"
		"6.  quit<no param>\n"
	};
	SendRspons(tcps, &rspns);

	//ѭ����ȡ�ͻ�������Ĳ����д���
	for(;;)
	{
		CmdPacket cmd;
		if(!ReceiveCmd(tcps, (char *)&cmd)) 
			break;
		if(!ProcessCmd(tcps, &cmd, &clientaddr)) 
			break;
	}
	
	//�߳̽���ǰ�رտ��������׽��֣�
	closesocket(tcps);
	delete lpParam;
	return 0; 
} 

int main(int argc, char* argv[])
{
	SOCKET tcps_listen;//FTP�������������������׽���
	struct threadData *pThInfo;

	if(!InitFTPSocket(&tcps_listen)) //FTP��ʼ��
		return 0;
	
	printf("Mini FTP Server listening on %d port...\n", CMD_PORT);
	
	//ѭ�����ܿͻ����������󣬲������߳�ȥ����
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
		//�ȴ����ܿͻ��˿�����������
		pThInfo->tcps = accept(tcps_listen, (SOCKADDR*)&pThInfo->clientaddress, &len);

		//����һ���߳���������Ӧ�ͻ��˵�����
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