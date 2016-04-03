//服务器侦听端口
#define CMD_PORT 8020
//客户端侦听端口
#define DATA_PORT 8021
//客户端命令报文缓存
#define CMD_PARAM_SIZE 256
//服务器回复报文缓存
#define RSPONS_TEXT_SIZE 512
#define BACKLOG 10
//数据缓存
#define DATA_BUFSIZE 8016

//命令类型
typedef enum 
{
	LS,CD,PWD,PUT,GET,QUIT
} ComandType;

//回复类型
typedef enum 
{
	OK,ERRORS
} RsponsType;

//命令报文，从客户端发往服务器
typedef struct _CmdPacket
{
	ComandType cmdtype;
	char param[CMD_PARAM_SIZE];
} CmdPacket;



//回复报文，从服务器发往客户端
typedef struct _RsponsPacket
{
	RsponsType rspnostype;
	char text[RSPONS_TEXT_SIZE];
} RsponsPacket;



