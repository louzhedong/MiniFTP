//�����������˿�
#define CMD_PORT 8020
//�ͻ��������˿�
#define DATA_PORT 8021
//�ͻ�������Ļ���
#define CMD_PARAM_SIZE 256
//�������ظ����Ļ���
#define RSPONS_TEXT_SIZE 512
#define BACKLOG 10
//���ݻ���
#define DATA_BUFSIZE 8016

//��������
typedef enum 
{
	LS,CD,PWD,PUT,GET,QUIT
} ComandType;

//�ظ�����
typedef enum 
{
	OK,ERRORS
} RsponsType;

//����ģ��ӿͻ��˷���������
typedef struct _CmdPacket
{
	ComandType cmdtype;
	char param[CMD_PARAM_SIZE];
} CmdPacket;



//�ظ����ģ��ӷ����������ͻ���
typedef struct _RsponsPacket
{
	RsponsType rspnostype;
	char text[RSPONS_TEXT_SIZE];
} RsponsPacket;



