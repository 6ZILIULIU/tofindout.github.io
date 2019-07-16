#include<stdio.h>
#include<stdlib.h>
#include<netinet/in.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<sys/time.h>
#include<arpa/inet.h>
#include<string.h>
#include<netinet/ip.h>
#include<netinet/ip_icmp.h>
#include<unistd.h>
#include<signal.h>
#include<errno.h>
#include<pthread.h> 
#include<netdb.h>

#define DEFAULT_PORT 8000
#define MAXLINE 4096

int main()
{   
    char buff[MAXLINE];
    int connect_fd;
    int sock = socket(AF_INET,SOCK_STREAM,0);
    struct sockaddr_in servaddr;
    
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(DEFAULT_PORT);
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    if(bind(sock,(struct sockaddr*)&servaddr,sizeof(servaddr))==-1)
    {
	printf("bind fail");
	exit(0);
    }
    if(listen(sock,1024)==-1)
    {
	printf("listen fail");
	exit(0);
    }
    printf("start working\n");
    while(1)
    {
        if((connect_fd=accept(sock,(struct sockaddr*)NULL,NULL))==-1){
	    printf("accept err :%s(error :%d)",strerror(errno),errno);
	    continue;
	}
    
    int n = recv(connect_fd,buff,MAXLINE,0);
    
    if(!fork())
    {
	if(send(connect_fd,"hello,you are connected!\n",26,0)==-1)
	perror("send_error");
	close(connect_fd);
	exit(0);
    }
    buff[n] = '\0';
    printf("client msg: %s\n",buff);
    close(connect_fd);
    }
    close(sock);
    return 0;
}
