/*用户需求：
1.显示当前在线用户
2.不同用户之间实现聊天通信
3.传输文件
*/

#include "myhead.h"
#include "feiq.h"


int main(int argc, char **argv)
{

	//初始化
	int sock4UDP = InitUDP();	
	int sock4TCP = InitTCP();
	service *serinfo = InitUser();
	serinfo->sock4UDP =  sock4UDP;
	serinfo->sock4TCP =  sock4TCP;

	get_username(serinfo);

	pthread_t procer, recver, iter, frecver;
	pthread_create(&procer,  NULL, process,          (void *)serinfo); //任务处理线程
	pthread_create(&recver,  NULL, receive, 		  (void *)serinfo);	//接收广播线程
  	pthread_create(&iter,       NULL, interact,  		  (void *)serinfo);	//交互线程
  	pthread_create(&frecver, NULL, take_recvlist_node,  (void *)serinfo);	//取接收链表队列成员线程

	login(serinfo);	//登录，发广播，通知上线

	//主线程发送文件
	while(1)
	{	
		struct sockaddr_in caddr;
		memset(&caddr, 0, sizeof(struct sockaddr_in));
		int size = sizeof(struct sockaddr);
		int newsockefd = accept(serinfo->sock4TCP, (struct sockaddr *)&caddr, &size);		//等待目标用户确认连接

		TCPpacket packet;
		memset(&packet, 0, sizeof(TCPpacket));
		if(recv(newsockefd, &packet, sizeof(TCPpacket), 0) == -1)	//接收目标用户的应答
		{
			perror("recv failed!");
		}

		//找到发送链表中对应的文件,扣下来准备发送出去
		bool file_exit = false;	//找到文件的标志位
		sendfile *phead = serinfo->sendhead;
		sendfile *psend  = phead->next;
		pthread_mutex_lock(&(serinfo->mutex4send));
		while(psend != NULL)
		{
			if(strcmp(packet.fileName, psend->fileName) == 0)	//匹配到文件名
			{
				file_exit = true;

				if(psend->next == NULL)
				{
					phead->next = NULL;
					(serinfo->send_queue_size)--;
					break;
				}

				phead->next = psend->next;
				psend->next = NULL;
				(serinfo->send_queue_size)--;
				break;
			}		

			if( psend->next == NULL  &&  (strcmp(packet.fileName, psend->fileName) != 0) )	 //匹配不到文件名
			{
				file_exit = false;
				printf("[%s]: is not found in send list!\n", packet.fileName);
			}

			phead = phead->next;
			psend = phead->next;
		}
		pthread_mutex_unlock(&(serinfo->mutex4send));

		
		if(file_exit == true)
		{
			printf("I have found the file!\n");

			//初始化一个数据包
			TCPpacket pack;
			memset(&pack, 0, sizeof(TCPpacket));
			strcpy(pack.fileName, psend->fileName);
			pack.filesize = psend->filesize;
			pack.seek = packet.seek;
			pack.TCPsock = newsockefd;

			pthread_t tid;
			pthread_create(&tid, NULL, TCPsendfile, (void *)&pack);   //创建分离线程发送文件   
			
			file_exit = false;
		}

		free(psend);
		
	}
	
	return 0;
}








