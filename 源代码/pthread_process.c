#include "myhead.h"
#include "feiq.h"

void *process(void *arg)	//任务处理线程，取命令信息结构体链表队列，扣节点
{
	service *serinfo  = (service *)arg;

	while(1)
	{
		//扣命令信息任务节点
		pthread_mutex_lock(&(serinfo->mutex1));	

		while(serinfo->cmd_queue_size == 0 && !(serinfo->shutdown))
		{
			//printf("[LINE]:%d\n", __LINE__);
			pthread_cond_wait(&(serinfo->cond1), &(serinfo->mutex1));
		}

		if(serinfo->cmd_queue_size == 0 && serinfo->shutdown == true)
		{
			//printf("[LINE]:%d\n", __LINE__);
			pthread_mutex_unlock(&(serinfo->mutex1));
			pthread_exit(NULL);
		}

		//取任务链表节点
		command *pcmd = serinfo->cmdhead->next;
		serinfo->cmdhead->next = serinfo->cmdhead->next->next;
		serinfo->cmd_queue_size--;
		pthread_mutex_unlock(&(serinfo->mutex1));


		//解释执行
		switch(pcmd->commandNo)
		{
			//接收到广播，有新的用户加入，把用户信息添加到用户信息结构体链表队列
			case  ADDUSER: 
						printf("[new user add]:%s\n", pcmd->senderName); 
						add_user(pcmd, serinfo);
						//把自己的ip等信息发回给广播者；
						pcmd->commandNo = SENDSELFIP;
						strncpy(pcmd->senderName, serinfo->login_user_name, strlen(serinfo->login_user_name)); 	//自己的用户名
						sendto(serinfo->sock4UDP, pcmd, sizeof(command), 0, (struct sockaddr*)&(pcmd->peer), sizeof(struct sockaddr));
						break;

			//接收到广播，有用户退出，把该用户从用户信息结构体链表中删除
			case LOGOUT:
						printf("[user logout]:%s\n", pcmd->senderName); 
						printf("[del ip]:%s\n", inet_ntoa(pcmd->peer.sin_addr));
						del_user(pcmd, serinfo);
						break;

			//接收到聊天信息，输出打印
			case CHAT:
						printf("[from %s]:%s\n", pcmd->senderName, pcmd->additional);
						break;

			case SENDSELFIP:
						if(strcmp(pcmd->senderName, serinfo->login_user_name) == 0)	//防止自己回发给自己
						{
							break;
						}
						add_user(pcmd, serinfo);
						break;

			//接收到对方发送文件命令，将接收文件加入接收文件队列，确认后接收
			case SENDFILE:
						printf("[recv]:%s\n", pcmd->additional);
						recvfile *recvnode = malloc(sizeof(recvfile));	//创建一个新的接收文件节点
						recvnode->next = NULL;
						recvnode->filesize = pcmd->sendnode.filesize;
						strcpy(recvnode->fileName, pcmd->sendnode.fileName);
						//复制地址

						recvnode->peer.sin_family = pcmd->peer.sin_family;
						recvnode->peer.sin_port = pcmd->peer.sin_port;
						recvnode->peer.sin_addr.s_addr = pcmd->peer.sin_addr.s_addr;	//复制发送者的地址

						add_recvnode(recvnode, serinfo);	//把接收文件节点加入到接收文件链表当中
						break;
		}

		//释放抠出来的节点的空间
		free(pcmd);

	}

}


void *receive(void *arg)	//接收UDP线程， 将任务加入命令信息结构体链表队列
{
	service *serinfo = (service *)arg;
	
	while(1)
	{
		//创建新的命令信息结构体节点并初始化
		command *newcmd =  malloc(sizeof(command));		
		newcmd->next = NULL;

		memset(&(newcmd->peer), 0, sizeof(struct sockaddr_in));
		int size = sizeof(struct sockaddr);
		recvfrom(serinfo->sock4UDP, newcmd, sizeof(command), 0, (struct sockaddr*)&(newcmd->peer), &size);


		//把节点挂上到命令信息结构体链表上
		pthread_mutex_lock(&(serinfo->mutex1));
		command *newcmdhead = serinfo->cmdhead;						
		if(newcmdhead->next == NULL)	//原来的任务链表没有节点	
		{
			newcmdhead->next = newcmd;
		}
		else	//原来的任务链表有节点		
		{
			while(newcmdhead->next != NULL)
			{
				newcmdhead = newcmdhead->next;
			}
			newcmdhead->next = newcmd;
		}

		(serinfo->cmd_queue_size)++;
		pthread_mutex_unlock(&(serinfo->mutex1));
		pthread_cond_signal(&(serinfo->cond1));

	}

}


void *interact(void *arg)		//交互线程
{
	service *serinfo = (service *)arg;

	char buf[MSGLEN];
	memset(buf, 0, MSGLEN);

	while(1)
	{
		//从终端输入命令
		gets(buf);
		printf("输入命令：%s\n", buf);
		//while(getchar()!='\n');

		//列出所有在线用户,打印在线用户信息
		if(strncmp(buf, "ls", 2) == 0)
		{
			user *puser = serinfo->usehead->next;
			printf("Online Users:\n");
			printf("==================================\n");
			printf("NO.\t\tNAME\t\tIP\n");

			//遍历每一个用户
			pthread_mutex_lock(&(serinfo->mutex2));
			while(puser != NULL)
			{
				printf("%-15d%-15s%-15s\n", puser->user_num, puser->name, inet_ntoa(puser->peer.sin_addr));
				puser = puser->next;
			}

			pthread_mutex_unlock(&(serinfo->mutex2));
			printf("==================================\n");

		}


		if(strncmp(buf, "talk", 4) == 0)
		{	
				printf("==================================\n");
				printf("Please enter the num of user:\n");

				char msg[MSGLEN];
				memset(msg, 0, MSGLEN);

				int num;
				scanf("%d", &num);
				while(getchar()!='\n');

				//遍历每一个用户
				user *puser = serinfo->usehead->next;
				pthread_mutex_lock(&(serinfo->mutex2));
				while(puser != NULL)
				{
					if(puser->user_num == num)		//找到要发送信息的目标用户
					{
						break;
					}
					puser = puser->next;
					if(puser->next == NULL && puser->user_num != num)		//找不到
					{
						printf("Sorry! this user is not exit!\nexit talk!\n");
						break;
					}
				}
				pthread_mutex_unlock(&(serinfo->mutex2));


				command  *cmd = malloc(sizeof(command));
				while(puser->user_num == num)
				{
					printf("[send to %s]:", puser->name);		
					gets(msg);
					//while(getchar()!='\n');

					if(strncmp(msg, "end", 3) == 0)
					{
						printf("\n");
						break;
					}

					cmd->commandNo = CHAT;
					strcpy(cmd->senderName, serinfo->login_user_name);
					strcpy(cmd->additional, msg);
					sendto(serinfo->sock4UDP, cmd, sizeof(command), 0, (struct sockaddr*)&(puser->peer), sizeof(struct sockaddr));
				}

				free(cmd);

		} 

		if(strncmp(buf, "send", 4) == 0)	//准备发送文件
		{
			printf("==================================\n");
			printf("Please enter the num of user:\n");

			int num;
			scanf("%d", &num);
			//fflush(stdin);
			while(getchar()!='\n');

			//遍历每一个用户
			user *puser = serinfo->usehead->next;
			pthread_mutex_lock(&(serinfo->mutex2));
			while(puser != NULL)
			{
				if(puser->user_num == num)		//找到要发送信息的目标用户
				{
					printf("Please enter the file you want to send:\n");

					char filename[FILELEN];
					memset(filename, 0, FILELEN);
					gets(filename);
					//while(getchar()!='\n');

					if(is_exit_file(filename,  "./") == false	)	//判断该文件是否存在于当前目录
					{
						printf("%s is not exit!\nexit send!\n", filename);
						break;
					}

					command *cmd  = malloc(sizeof(command));
					cmd->commandNo = SENDFILE;
					cmd->sendnode.next = NULL;
					strcpy(cmd->senderName, serinfo->login_user_name); 
					strcpy(cmd->additional, "send file!"); 

					add_fileinfo2sendnode(filename, &(cmd->sendnode));		//把文件大小和名字信息存放到发送文件节点
					add_addr2sendnode(puser, &(cmd->sendnode));				//把目标用户的地址存放到发送文件节点

					add_sendnode(&(cmd->sendnode), serinfo);	//添加发送文件节点到发送队列
					sendto(serinfo->sock4UDP, cmd, sizeof(command), 0, (struct sockaddr*)&(puser->peer), sizeof(struct sockaddr)); //把命令包发送给目标用户

					free(cmd);
					break;
				}

				puser = puser->next;
				if(puser->next == NULL && puser->user_num != num)		//找不到目标用户
				{
					printf("Sorry! this user is not exit!\nexit send!\n");
					break;
				}
			}
			pthread_mutex_unlock(&(serinfo->mutex2));

		}

		if(strncmp(buf, "yes", 3) == 0)
		{
			serinfo->fileok = true;
			pthread_cond_signal(&(serinfo->cond4file));	//发送信号唤醒线程确认接收文件
		}

		if(strncmp(buf, "no", 2) == 0)
		{
			serinfo->fileok = false;
			pthread_cond_signal(&(serinfo->cond4file));	//发送信号唤醒线程拒绝接收文件
		}


		if(strncmp(buf, "over", 4) == 0)
		{
			logout(serinfo);
		}


		memset(buf, 0, MSGLEN);
	}

}


void *TCPsendfile(void *arg)		//这是一个TCP发送文件的分离线程
{
	TCPpacket *ptr = (TCPpacket *)arg;

	TCPpacket  packet;
	memset(&packet, 0, sizeof(TCPpacket));
	memset(packet.buf, 0, DATALENTH);
	strcpy(packet.fileName, ptr->fileName);
	packet.filesize = ptr->filesize;
	packet.TCPsock = ptr->TCPsock;
	packet.seek = ptr->seek;
	packet.file_end = false;

	printf("Filename:%s\n" ,packet.fileName); 
	printf("Size:%ld\n", packet.filesize);
	printf("TCPsock:%d\n", packet.TCPsock);

	int fd = open(packet.fileName, O_RDONLY);
	if(fd == -1)
	{
		perror("open file failed!");
	}

	lseek(fd, packet.seek, SEEK_SET);		//设置读写文件位置
	printf("Current seek:%ld\n", packet.seek);

	int size;
	while(1)
	{
		if(packet.seek == packet.filesize)		//如果文件已经传输完成了，则退出
		{
			printf("The file had sent!exit send!\n");
			break;
		}

		size = read(fd, packet.buf, DATALENTH);
		if(size == -1)
		{	
			perror("read failed!");
			close(fd);
			break;
		}
		else if(size == 0)
		{
			printf("read file done!\n");
			packet.file_end = true;
			send(packet.TCPsock, &packet, sizeof(TCPpacket), 0);
			break;
		}

		packet.num = size; //读到的字节数
		printf("sending....[%d]bytes\n", packet.num);
		if(send(packet.TCPsock, &packet, sizeof(TCPpacket), 0) == -1)
		{
			printf("Transmission interrupted!");
			break;
		}

		memset(packet.buf, 0, DATALENTH);
		packet.num = 0;

		sleep(1);
	}

	pthread_detach(pthread_self());	
	printf("发送线程分离\n");

}




void *take_recvlist_node(void *arg)	//取接收链表队列成员线程
{
	service *serinfo = (service *)arg;

	while(1)
	{
		//扣命令信息任务节点
		pthread_mutex_lock(&(serinfo->mutex4recv));	

		while(serinfo->recv_queue_size == 0 && !(serinfo->shutdown))
		{
			//printf("[LINE]:%d\n", __LINE__);
			pthread_cond_wait(&(serinfo->cond4recv), &(serinfo->mutex4recv));
		}

		if(serinfo->recv_queue_size == 0 && serinfo->shutdown == true)
		{
			//printf("[LINE]:%d\n", __LINE__);
			pthread_mutex_unlock(&(serinfo->mutex4recv));
			pthread_exit(NULL);
		}

		//取任务链表节点
		recvfile *precv = serinfo->recvhead->next;
		serinfo->recvhead->next = serinfo->recvhead->next->next;
		serinfo->recv_queue_size--;
		precv->next = NULL;

		printf("=======================================\n");
		printf("From:%s\n", inet_ntoa(precv->peer.sin_addr));
		printf("Filename:%s\n" ,precv->fileName); 
		printf("Size:%ld\n", precv->filesize);
		printf("Would you want to receive (yes/no)?\n");
		printf("=======================================\n");

		pthread_cond_wait(&(serinfo->cond4file), &(serinfo->mutex4recv));		//等待信号确认接收文件

		if(serinfo->fileok == true)	//	确认接收文件
		{
			int newsock4TCP = socket(AF_INET, SOCK_STREAM, 0);	//创建准备接收数据的数据流新socket
			if(newsock4TCP == -1)
			{
				perror("create new TCP socket failed!");
			}

			if(connect(newsock4TCP, (struct sockaddr*)&(precv->peer), sizeof(struct sockaddr)) == -1)	//与发送者进行TCP连接
			{
				perror("connect failed!");
			}


			//初始化发送包
			TCPpacket packet;
			memset(&packet, 0, sizeof(TCPpacket));
			memset(packet.buf, 0, DATALENTH);
			strcpy(packet.fileName, precv->fileName);
			packet.filesize = precv->filesize;
			packet.TCPsock = newsock4TCP;
			packet.file_end = false;

			int fd = -1;
			if(access(packet.fileName, F_OK) == 0)  //文件已存在
			{
				fd = open(packet.fileName, O_RDONLY);
				if(fd == -1)
				{
					perror("open file failed!");
				}
				packet.seek = lseek(fd, 0, SEEK_END);		//当前文件位置
				printf("当前文件位置seek:%ld\n", packet.seek);
			}
			else
			{
				packet.seek = 0;	//设置文件的偏移量为0
			}
			close(fd);


			send(newsock4TCP, &packet, sizeof(TCPpacket), 0);	//应答发送者

			pthread_t tid;
			pthread_create(&tid, NULL, TCPrecvfile, (void *)&packet);   //创建分离线程接收文件   

			serinfo->fileok = false;

		}

		if(serinfo->fileok == false)		//不接收文件	
		{
			free(precv);
		}

		pthread_mutex_unlock(&(serinfo->mutex4recv));

	}
}


void *TCPrecvfile(void *arg)		//这是一个TCP接收文件的分离线程
{
	TCPpacket *ptr = (TCPpacket *)arg;

	//初始化一个数据包
	TCPpacket  packet;
	memset(&packet, 0, sizeof(TCPpacket));
	memset(packet.buf, 0, DATALENTH);
	strcpy(packet.fileName, ptr->fileName);
	packet.filesize = ptr->filesize;
	packet.TCPsock = ptr->TCPsock;
	packet.seek = ptr->seek;
	packet.file_end = false;

	printf("Filename:%s\n" ,packet.fileName); 
	printf("Size:%ld\n", packet.filesize);
	printf("TCPsock:%d\n", packet.TCPsock);
	printf("Current seek:%ld\n", packet.seek);

	int fd = -1;
	if(access(packet.fileName, F_OK) == 0)  //文件已存在
	{
		fd = open(packet.fileName, O_WRONLY|O_APPEND, 0777);
		if(fd == -1)
		{
			perror("open file failed!");
		}
		lseek(fd, packet.seek, SEEK_SET);	//设置文件的偏移量
	}
	else	//文件不存在
	{
		fd = open(packet.fileName, O_WRONLY|O_CREAT, 0777);
		if(fd == -1)
		{
			perror("open file failed!");
		}
	}


	int ret;
	while(1)
	{
		if(packet.seek == packet.filesize)		//如果文件已经传输完成
		{
			printf("The file had recvived! exit recv!\n");
			break;
		}

		//char buf[1024] = {0};
		ret = recv(ptr->TCPsock, &packet, sizeof(TCPpacket), 0);
		if(packet.file_end == true)
		{
			break;
		}
		//memcpy(&packet, buf, sizeof(TCPpacket));

		if(ret == -1)
		{
			close(ptr->TCPsock);
			perror("TCP recv failed!\n");
			break;
		}
		else if(ret == 0)
		{
			printf("communicate interrupted!\n");
			close(ptr->TCPsock);
			break;
		}

		char *tmp = packet.buf;
		int num = packet.num;
		printf("recv...num:[%d]bytes\n", num);
		int wnum;
		while(num>0)
		{
			if((wnum = write(fd, tmp, num)) == -1)
			{
				perror("write failed!");
				break;
			}
			tmp += wnum;
			num -= wnum;
		}		


		memset(packet.buf, 0, DATALENTH);

		sleep(1);
	}
	
	close(fd);
	pthread_detach(pthread_self());
	printf("接收文件线程分离\n");
}


