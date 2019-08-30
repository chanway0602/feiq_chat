#ifndef  _FEIQ_H_
#define _FEIQ_H_


#include "myhead.h"


#define  ADDUSER   1
#define  LOGOUT      2
#define  CHAT           3
#define  SENDSELFIP        4
#define  SENDFILE    5

#define  NAMELEN   50        //用户名长度
#define  MSGLEN    120       //命令包消息长度
#define  FILELEN     50        //文件名长度
#define  DATALENTH  200   //传输文件包的buf长度

typedef struct TCPpacket 
{
  short int num;  //读到的字节数
  off_t filesize;
  off_t seek;
  int TCPsock;
  bool file_end;
  char fileName[NAMELEN];
  char buf[DATALENTH];

}TCPpacket;


//发送文件结构体节点
typedef struct sendfile
{
  char fileName[NAMELEN];
  off_t filesize;
  off_t seek;
  struct sockaddr_in peer;  //目标用户的地址
  struct sendfile *next;
  
}sendfile;



//接收文件结构体节点
typedef struct recvfile
{
  char fileName[NAMELEN];
  off_t filesize;
  off_t seek;
  struct sockaddr_in peer;  //发送者的地址
  struct recvfile *next;

}recvfile;



//UDP命令信息结构体节点
typedef struct command
{
  unsigned int commandNo;                 //命令类型，是广播，聊天还是传输文件

  char         senderName[NAMELEN];
  char         senderHost[NAMELEN];
  char         additional[MSGLEN];		//传输的内容信息

  sendfile sendnode;
  recvfile  recvnode;
  
  struct sockaddr_in peer;  //发送者的地址
  struct command *next;
  
}command;


//在线用户信息链表节点
typedef struct user
{
  char   name[NAMELEN];
  char   host[NAMELEN];
  char   nickname[NAMELEN];

  int    inUse;
  int    exit;
  int    user_num;

  struct user *next;
  struct sockaddr_in peer;
} user;





//信息管理结构体
typedef struct service
{
	int sock4UDP;
	int sock4TCP;
	int  cmd_queue_size;
  int  user_queue_size;
  int  send_queue_size;
  int  recv_queue_size;

  char login_user_name[NAMELEN];

	user *usehead;
	command *cmdhead;

  sendfile *sendhead;
  recvfile  *recvhead;

	pthread_mutex_t mutex1;  //命令信息结构体链表的锁
	pthread_mutex_t mutex2;  //用户信息结构体链表的锁
  pthread_mutex_t mutex4send; //发送文件队列的锁
  pthread_mutex_t mutex4recv;  //接收文件队列的锁
	pthread_cond_t   cond1; //命令信息结构体链表的条件变量
  pthread_cond_t   cond2; //用户信息结构体链表的条件变量
  pthread_cond_t   cond4recv; //接收文件队列的条件变量
  pthread_cond_t   cond4file; //确认是否接收文件的条件变量

  bool fileok;
	bool shutdown;

}service;



int InitUDP(void);
int InitTCP(void);

service *InitUser(void);

void *process(void *arg);
void *receive(void *arg);
void *interact(void *arg);
void *take_recvlist_node(void *arg);  //取接收链表队列成员线程
void *TCPsendfile(void *arg);    //这是一个TCP发送文件的分离线程
void *TCPrecvfile(void *arg);    //这是一个TCP接收文件的分离线程


void add_user(command *pcmd, service *serinfo);
void del_user(command *pcmd, service *serinfo);

void login(service *serinfo);
void logout(service *serinfo);


void get_username(service *serinfo);

void add_fileinfo2sendnode(char *filename, sendfile *sendnode);
void add_addr2sendnode(user *puser, sendfile *sendnode);
void add_sendnode(sendfile *sendnode, service *serinfo);
void add_recvnode(recvfile *recvnode, service *serinfo);

bool is_exit_file(char *filename, char *dir);




#endif
