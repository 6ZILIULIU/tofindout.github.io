#include<stdio.h>
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

typedef struct pingm_packet{
	struct timeval tv_begin;
	struct timeval tv_end;
	short seq;
	int flag;
}pingm_packet;
static pingm_packet pingpacket[128];



static pingm_packet* icmp_findpacket(int seq);
static unsigned short icmp_cksum(unsigned char *data, int len);
static struct timeval icmp_tvsub(struct timeval end, struct timeval begin);
static void icmp_statistics(void);
static void icmp_pack(struct icmp *icmph, int seq ,struct timeval *tv ,int length);
static int icmp_unpack(char *buf,int len);
static void *icmp_send(void *argc);
static void *icmp_recv(void *argv);
static void icmp_sigint(int signo);
static void icmp_usage();
#define K 1024
#define BUFFERSIZE 72
static unsigned char send_buff[BUFFERSIZE];
static unsigned char recv_buff[2*K];
static struct sockaddr_in dest;
static int rawsock = 0;
static pid_t pid = 0;
static int alive = 0;
static short packet_send = 0;
static short packet_recv = 0;
static char dest_str[80];
static struct timeval tv_begin , tv_end, tv_interval;

static void icmp_usage()
{
	/*ping加ip地址或者域名*/
	printf("ping aaa.bbb.ccc.ddd\n");
}

static unsigned short icmp_cksum(unsigned char *data ,int len)
{
	int sum = 0;
	int odd = len & 0x01;
	while(len & 0xfffe)
	{
		sum += *(unsigned short*)data;
		data += 2;
		len -= 2;
	}
	if(odd)
	{
		unsigned short tmp = ((*data)<<8&0xff00);
		sum += tmp;
	}

	sum = (sum >>16)+(sum & 0xffff);
	sum += (sum>>16);
	return (~sum);
}


static void icmp_pack(struct icmp *icmph,int seq,struct timeval *tv, int length)
{
	unsigned char i = 0;
	/*设置icmp报头*/
	icmph->icmp_type = ICMP_ECHO;
	icmph->icmp_code = 0;
	icmph->icmp_cksum = 0;
	icmph->icmp_seq = seq;
	icmph->icmp_id = pid & 0xffff;
	for(i = 0;i<length;i++)
	{
		icmph->icmp_data[i]=i;
	}
	icmph->icmp_cksum = icmp_cksum((unsigned char*)icmph,length);
}

static int icmp_unpack(char *buf , int len)
{
	int i,iphdrlen;
	struct ip *ip =NULL;
	struct icmp *icmp = NULL;
	int rtt;

	ip = (struct ip*)buf;
	iphdrlen = ip->ip_hl*4;
	icmp=(struct icmp*)(buf+iphdrlen);
	len-=iphdrlen;

	if(len<8)
	{
		printf("icmp packet\'s length is less than 8\n");
		return -1;
	}
	
	if((icmp->icmp_type == ICMP_ECHOREPLY) && (icmp->icmp_id==pid))
	{
		struct timeval tv_internel , tv_recv ,tv_send;
		//在发送列表中查找已经发送的包，按照seq*
		pingm_packet *packet = icmp_findpacket(icmp->icmp_seq);
		if(packet ==NULL)
		    return -1;
		packet->flag = 0;
		tv_send = packet->tv_begin;

		gettimeofday(&tv_recv,NULL);
		tv_internel = icmp_tvsub(tv_recv,tv_send);
		rtt = tv_internel.tv_sec*1000+tv_internel.tv_usec/1000;

		printf("%d byte from %s : icmp_seq = %u ttl=%d rtt=%d ms\n",
			len,
			inet_ntoa(ip->ip_src),
			icmp->icmp_seq,
			ip->ip_ttl,
			rtt);
		packet_recv ++;
	}
	else
	{
	    return -1;
	}
}
/*查找数组中的标识函数icmp_findpacket*/
static pingm_packet* icmp_findpacket(int seq)
{
	int i =0;
	pingm_packet *found =NULL;
	/*查找包的位置*/
	if(seq==-1)
	{
		for(i=0;i<128;i++)
		{
			if(pingpacket[i].flag==0)
			{
				found = &pingpacket[i];
				break;
			}
		}
	}
	else if(seq >=0)
	{
		for(i=0;i<128;i++)
		{
			if(pingpacket[i].seq== seq)
			{
				found = &pingpacket[i];
				break;
			}
		}
	}
	return found;
}






static void *icmp_send(void *argc)
{
    gettimeofday(&tv_begin,NULL);
    while(alive)
    {
	int size = 0 ;
	struct timeval tv;
	gettimeofday(&tv,NULL);  //当前包的发送时间
	//在发送包状态数组中找到一个空闲位置
	pingm_packet *packet = icmp_findpacket(-1);
	if(packet)
	{
	    packet->seq = packet_send;
	    packet->flag = 1;
	    gettimeofday(&packet->tv_begin,NULL);
	}

	icmp_pack((struct icmp*)send_buff,packet_send ,&tv ,64 );
	//打包数据
	size = sendto(rawsock,send_buff,64,0,(struct sockaddr*)&dest ,sizeof(dest));

	if(size<0)
	{
	    perror("sendto error");
	    continue;
	}

	packet_send++;
	/*每隔1s发送一个icmp回显请求包*/
	sleep(1);
    }
}

static void *icmp_recv(void *argv)
{
    /*轮询等待时间*/
    struct timeval tv;
    tv.tv_usec = 200;
    tv.tv_sec = 0;
    fd_set readfd;

    while(alive)
    {
	int ret = 0;
	FD_ZERO(&readfd);
	FD_SET(rawsock,&readfd);
	ret = select(rawsock+1,&readfd ,NULL,NULL,&tv);
	switch(ret)
	{
	    case -1:
		/*错误发生*/
		break;
	    case 0:
		/*超时*/
		break;
	    default:
		{
		    /*收到一个包*/
		    int fromlen = 0;
		    struct sockaddr from;
		    /*接收数据*/
		    int size = recv(rawsock,recv_buff,sizeof(recv_buff),0);
		    if(errno == EINTR)
		    {
			perror("recvfrom error");
			continue;
		    }
		    /*解包，并设置相关变量*/
		    ret = icmp_unpack(recv_buff,size);
		    if(ret == -1)
		    {
			continue;
		    }
		    break;
		}
	}
    }
}

static struct timeval icmp_tvsub(struct timeval end ,struct timeval begin)
{
    struct timeval tv;
    //计算差值
    tv.tv_sec = end.tv_sec - begin.tv_sec;
    tv.tv_usec = end.tv_usec - begin.tv_usec;
    //如果接收时间的usec值小于发送时的usec值，从usec域错位
    if(tv.tv_usec < 0)
    {
	tv.tv_sec --;
	tv.tv_usec += 1000000;
    }
    return tv;
}

static void icmp_sigint(int signo)
{
    alive = 0;
    gettimeofday(&tv_end ,NULL);
    tv_interval = icmp_tvsub(tv_end , tv_begin);

    return;
}

static void icmp_statistics(void)
{
    long time = (tv_interval.tv_sec*1000)+(tv_interval.tv_usec/1000);
    printf("--- %s ping statistics ---\n",dest_str);
    printf("%d packets transmitted,%d received , %d %% packet loss, time %d ms\n",
	    packet_send,
	    packet_recv,
	    (packet_send-packet_recv)*100/packet_send,
	    time
	    );
}

int main(int argc, char *argv[])
{	
	struct hostent * host = NULL;
	struct protoent *protocol = NULL;
	char protoname[]="icmp";
	unsigned long inaddr = 1;
	int size = 128*K;
				    /*参数是否数量正确*/
	if(argc<2)
	{
	    icmp_usage();
	    return -1;
	}
	protocol = getprotobyname(protoname);
	if(protocol == NULL)
	{
	    perror("getprotobyname()");
	    return -1;
	}
	
	memcpy(dest_str, argv[1], strlen(argv[1])+1);
	memset(pingpacket,0,sizeof(pingm_packet) * 128);

	rawsock = socket(AF_INET,SOCK_RAW,protocol->p_proto);
	if(rawsock<0)
	{
	    perror("socket");
	    return -1;
	}
	
	pid = getuid();

	setsockopt(rawsock, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size));
	bzero(&dest, sizeof(dest));

	dest.sin_family = AF_INET;

	inaddr = inet_addr(argv[1]);
	if(inaddr ==INADDR_NONE)
	{
	    host = gethostbyname(argv[1]);
	    if(host == NULL)
	    {
		perror("gethostbyname");
		return -1;
	    }

	    memcpy((char *)&dest.sin_addr, host->h_addr, host->h_length);
	}
	else
	{
	    memcpy((char*)&dest.sin_addr, &inaddr, sizeof(inaddr));
	}

	inaddr = dest.sin_addr.s_addr;
	printf("ping %s (%d.%d.%d.%d) 56(84) bytes of data. \n",
		dest_str,
		(inaddr&0x000000ff)>>0,
		(inaddr&0x0000ff00)>>8,
		(inaddr&0x00ff0000)>>16,
		(inaddr&0xff000000)>>24);

	signal(SIGINT, icmp_sigint);

	alive = 1;
	pthread_t send_id, recv_id;
	int err = 0;
	err = pthread_create(&send_id,NULL,(void*)icmp_send,NULL);

	if(err < 0)
	{
	    return -1;
	}
	err = pthread_create(&recv_id,NULL,(void*)icmp_recv,NULL);
	if(err < 0)
	{
	    return -1;
	}

	pthread_join(send_id,NULL);
	pthread_join(recv_id,NULL);

	close(rawsock);
	icmp_statistics();
	return 0;
}
