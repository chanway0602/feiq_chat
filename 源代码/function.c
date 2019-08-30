#include "myhead.h"
#include "feiq.h"


int InitTCP(void)
{
	//配置TCP
	int sock4TCP = socket(AF_INET, SOCK_STREAM, 0);	//创建数据流socket
	if(sock4TCP == -1)
	{
		perror("create TCP socket failed!");
	}

	int sinsize = 1;
	setsockopt(sock4TCP, SOL_SOCKET, SO_REUSEADDR, &sinsize, sizeof(int));	//设置socket属性

	struct sockaddr_in myaddr;
	memset(&myaddr, 0, sizeof(struct sockaddr_in));
	myaddr.sin_family = AF_INET;
	myaddr.sin_port = htons(6666);
	myaddr.sin_addr.s_addr = htonl(INADDR_ANY);  

	if(bind(sock4TCP, (struct sockaddr *)&myaddr, sizeof(struct sockaddr)) == -1)		//绑定ip和端口
	{
		perror("UDP bind failed!");
	}

	if(listen(sock4TCP, 5) == -1)	//打开监听
	{
		perror("TCP listen failed!");
	}

	return sock4TCP;

}


int InitUDP(void)
{
	//配置UDP
	int sock4UDP = socket(AF_INET, SOCK_DGRAM, 0);	//1.创建数据报socket
	if(sock4UDP == -1)
	{
		perror("create UDP socket failed!");
	}

	struct sockaddr_in myaddr;
	memset(&myaddr, 0, sizeof(struct sockaddr_in));
	myaddr.sin_family = AF_INET;
	myaddr.sin_port = htons(6666);
	myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	if(bind(sock4UDP, (struct sockaddr*)&myaddr, sizeof(struct sockaddr)) == -1)		//2.绑定本机IP和端口号
	{
		perror("UDP bind failed!");
	}

	int sinsize=1;
	setsockopt(sock4UDP, SOL_SOCKET, SO_BROADCAST, &sinsize, sizeof(int)); 	//3.打开广播功能

	return sock4UDP;

}


service *InitUser(void)
{
	service *serinfo = calloc(1, sizeof(service));

	serinfo->usehead   = malloc(sizeof(user));
	serinfo->cmdhead  = malloc(sizeof(command));
	serinfo->sendhead = malloc(sizeof(sendfile));
	serinfo->recvhead  = malloc(sizeof(recvfile));

	serinfo->usehead->next  = NULL;
	serinfo->cmdhead->next  = NULL;
	serinfo->sendhead->next = NULL;
	serinfo->recvhead->next  = NULL;

	serinfo->shutdown = false;
	serinfo->fileok       = false;
	serinfo->cmd_queue_size  = 0;
	serinfo->user_queue_size  = 0;
	serinfo->send_queue_size = 0;
	serinfo->recv_queue_size  = 0;

	memset(serinfo->login_user_name, 0, NAMELEN);

	//互斥锁初始化
	if(pthread_mutex_init(&(serinfo->mutex1), NULL) != 0)
	{
		perror("mutex1 init failed!");
	}
	if(pthread_mutex_init(&(serinfo->mutex2), NULL) != 0)
	{
		perror("mutex2 init failed!");
	}
	if(pthread_mutex_init(&(serinfo->mutex4send), NULL) != 0)
	{
		perror("mutex4send init failed!");
	}
	if(pthread_mutex_init(&(serinfo->mutex4recv), NULL) != 0)   
	{
		perror("mutex4recv init failed!");
	}



	//条件变量初始化
	if(pthread_cond_init(&(serinfo->cond1), NULL) != 0)
	{
		perror("cond1 init failed!");
	}
	if(pthread_cond_init(&(serinfo->cond2), NULL) != 0)
	{
		perror("cond2 init failed!");				
	}
	if(pthread_cond_init(&(serinfo->cond4recv), NULL) != 0)
	{
		perror("cond4recv init failed!");
	}
	if(pthread_cond_init(&(serinfo->cond4file), NULL) != 0)
	{
		perror("cond4file init failed!");
	}


	return serinfo;
}


void add_recvnode(recvfile *recvnode, service *serinfo)
{
	recvnode->next = NULL;
	recvfile *newrecvhead = serinfo->recvhead;

	//把发送信息节点挂到发送队列上
	pthread_mutex_lock(&(serinfo->mutex4recv));
	if(newrecvhead->next == NULL)	//原来的任务链表没有节点	
	{
		newrecvhead->next = recvnode;
	}
	else	//原来的任务链表有节点		
	{
		while(newrecvhead->next != NULL)
		{
			newrecvhead = newrecvhead->next;
		}
		newrecvhead->next = recvnode;
	}
	(serinfo->recv_queue_size)++;
	pthread_mutex_unlock(&(serinfo->mutex4recv));

	pthread_cond_signal(&(serinfo->cond4recv));	//发送信号唤醒线程接收文件

}

void del_user(command *pcmd, service *serinfo)
{
	user *phead = serinfo->usehead;
	user *puser  = phead->next;

	//删除用户信息结构体链表上的某一个指定用户
	pthread_mutex_lock(&(serinfo->mutex2));
	while(puser != NULL)
	{
		//匹配地址
		//if(strcmp(inet_ntoa(puser->peer.sin_addr), inet_ntoa(pcmd->peer.sin_addr)) == 0)
		//匹配名字
		if(strcmp(puser->name, pcmd->senderName) == 0)
		{
			if(puser->next == NULL)
			{
				phead->next = NULL;
				free(puser);
				(serinfo->user_queue_size)--;
				break;
			}
			phead->next = puser->next;
			puser->next = NULL;
			free(puser);
			(serinfo->user_queue_size)--;
			break;
		}

		phead = phead->next;
		puser = phead->next;
	}

	pthread_mutex_unlock(&(serinfo->mutex2));
}


void add_user(command *pcmd, service *serinfo)
{
	static int num = 1;

	user *newuserhead = serinfo->usehead;

	user *newuser = malloc(sizeof(user));	//初始化一个新的用户信息节点
	newuser->next = NULL;
	newuser->user_num = num++;

	strcpy(newuser->name, pcmd->senderName);
	memset(&(newuser->peer), 0, sizeof(struct sockaddr_in));
	newuser->peer.sin_family = pcmd->peer.sin_family;
	newuser->peer.sin_port = pcmd->peer.sin_port;
	newuser->peer.sin_addr.s_addr = pcmd->peer.sin_addr.s_addr;


	//把节点挂上到用户信息结构体链表上
	pthread_mutex_lock(&(serinfo->mutex2));
	if(newuserhead->next == NULL)	//原来的任务链表没有节点	
	{
		newuserhead->next = newuser;
	}
	else	//原来的任务链表有节点		
	{
		while(newuserhead->next != NULL)
		{
			newuserhead = newuserhead->next;
		}
		newuserhead->next = newuser;
	}
	(serinfo->user_queue_size)++;
	pthread_mutex_unlock(&(serinfo->mutex2));

	//printf("[newus ip]:%s\n", inet_ntoa(newuser->peer.sin_addr));

	//pthread_cond_signal(&(serinfo->cond1));

}

void add_sendnode(sendfile *sendnode, service *serinfo)
{
	sendfile *newsendnode = malloc(sizeof(sendfile));	//建立一个新的发送节点

	strcpy(newsendnode->fileName, sendnode->fileName);
	newsendnode->filesize = sendnode->filesize;
	newsendnode->seek = sendnode->seek;
	newsendnode->next = NULL;

	sendfile *newsendhead = serinfo->sendhead;

	//把发送信息节点挂到发送队列上
	pthread_mutex_lock(&(serinfo->mutex4send));
	if(newsendhead->next == NULL)	//原来的任务链表没有节点	
	{
		newsendhead->next = newsendnode;
	}
	else	//原来的任务链表有节点		
	{
		while(newsendhead->next != NULL)
		{
			newsendhead = newsendhead->next;
		}
		newsendhead->next = newsendnode;
	}
	(serinfo->send_queue_size)++;
	pthread_mutex_unlock(&(serinfo->mutex4send));

	//pthread_cond_signal(&(serinfo->mutex4send));	

}


void add_fileinfo2sendnode(char *filename, sendfile *sendnode)
{
	struct stat fileinfo;
	if(stat(filename, &fileinfo) != 0)	//当前目录下的filename
	{
		perror("stat failed!");
	}

	strcpy(sendnode->fileName, filename);		//文件名
	sendnode->filesize = fileinfo.st_size;	//大小

}

void add_addr2sendnode(user *puser, sendfile *sendnode)
{

	sendnode->peer.sin_family = puser->peer.sin_family;
	sendnode->peer.sin_port = puser->peer.sin_port;
	sendnode->peer.sin_addr.s_addr = puser->peer.sin_addr.s_addr;	//地址

}


bool is_exit_file(char *filename, char *dir)
{
	DIR *dirp = NULL;
	if( (dirp = opendir(dir) ) == NULL)
	{
		perror("opendir failed!");
	}

	struct dirent *ptr = NULL;
	while( (ptr = readdir(dirp) ) != NULL)
	{
		if(strcmp(ptr->d_name,".") == 0 || strcmp(ptr->d_name,"..") == 0)
		{
			continue;
		}

		if(strcmp(ptr->d_name, filename) == 0)
		{
			return true;
		}

	}

	return false;

	closedir(dirp);
}



void login(service *serinfo)	  //登录，发广播，通知上线
{
	command *logcmd = malloc(sizeof(command));
	logcmd->commandNo = ADDUSER;
	logcmd->next = NULL;
	strncpy(logcmd->senderName, serinfo->login_user_name, strlen(serinfo->login_user_name));	//用户名
	strcpy(logcmd->additional, "I am login!");
	//strcpy(logcmd->senderHost, "");

	memset(&(logcmd->peer), 0, sizeof(struct sockaddr_in));
	logcmd->peer.sin_family = AF_INET;
	logcmd->peer.sin_port = htons(6666);
	logcmd->peer.sin_addr.s_addr = inet_addr("192.168.14.255");

	sendto(serinfo->sock4UDP, logcmd, sizeof(command), 0, (struct sockaddr*)&(logcmd->peer), sizeof(struct sockaddr));
	printf("login successfully, broadcasted!\n");

	free(logcmd);

}

void logout(service *serinfo)	//退出登录，发广播通知下线，释放相关资源
{
	//发广播
	command *logcmd = calloc(1, sizeof(command));
	logcmd->commandNo = LOGOUT;
	logcmd->next = NULL;
	strncpy(logcmd->senderName, serinfo->login_user_name, strlen(serinfo->login_user_name));	//用户名
	strcpy(logcmd->additional, "I am logout!");

	memset(&(logcmd->peer), 0, sizeof(struct sockaddr_in));
	logcmd->peer.sin_family = AF_INET;
	logcmd->peer.sin_port = htons(6666);
	logcmd->peer.sin_addr.s_addr = inet_addr("192.168.14.255");

	sendto(serinfo->sock4UDP, logcmd, sizeof(command), 0, (struct sockaddr*)&(logcmd->peer), sizeof(struct sockaddr));
	printf("logout successfully, broadcasted!\n");

	free(logcmd);

	//sleep(1);
	exit(0);
	//释放资源

}

void get_username(service *serinfo)
{
	printf("=============================\n");
	printf("please enter your login name:\n");
	char name[NAMELEN];
	memset(name, 0, NAMELEN);
	gets(name);
	printf("=============================\n");

	strcpy(serinfo->login_user_name, name);
}