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

#define MAXLINE 4096

int main(int argc,char** argv)
{
    int sock ,recv_len;
    char recvline[4096], sendline[4096];
    char buf[MAXLINE];
    struct sockaddr_in servaddr;
    if(argc!=2){
	printf("usage: ./tcpclient <ipaddress>\n");
	exit(0);
    }
    sock = socket(AF_INET,SOCK_STREAM,0);

    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(8000);
    if(inet_pton(AF_INET,argv[1],&servaddr.sin_addr.s_addr)<= 0)
    {
	printf("inet_pton error for %s \n",argv[1]);
	exit(0);
    }
    if(connect(sock,(struct sockaddr*)&servaddr,sizeof(servaddr))<0){
	printf("connect fail");
	exit(0);
    }
    
    printf("send msg:\n");
    fgets(sendline,4096,stdin);
    if((send(sock,sendline,strlen(sendline),0))<0){
	printf("send msg too less");
	exit(0);
    }

    recv_len = recv(sock,buf,MAXLINE,0);
    buf[recv_len] = '\0';
    printf("recv : %s",buf);
    
    close(sock);
    exit(0);

    return 0;
}

