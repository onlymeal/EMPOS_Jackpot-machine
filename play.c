#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <termios.h>

#define GAMBLE_PRINT	0
#define GAMBLE_MONEY	1
#define GAMBLE_BAT	2
#define GAMBLE_RESULT 	3

static int dev, count;
static unsigned char vkey;

static int segflag[4];
static int segval[4];

void usrsignal(int sig){
	read(dev, &vkey,1);
}

void gamble_init(){
	int i;
	for(i=0;i<4;i++){
		segflag[i]=0;
	}
}

void gamble_batting(){
	while(1){
	ioctl(dev,GAMBLE_MONEY,0,sizeof(int));
	if(vkey==0x02){
		ioctl(dev,GAMBLE_BAT,0,sizeof(int));
		vkey=0x00;
		usleep(1);
	}
	if(vkey==0x01)
		break;
	}
	
}

void gamble_result(){
	int num=0, i, j;
	for(i=0; i<4; i++)
		for(j=i+1; j<4; j++)
			if(segval[i]==segval[j])
				num++;
	ioctl(dev,GAMBLE_RESULT,&num,sizeof(int));
	usleep(10);
	count++;
}

int main(void)
{
	pid_t id;
	unsigned int k=100;
	int segvalue,j;	
	dev=open("/dev/gamble", O_RDWR);

	(void)signal(SIGUSR1,usrsignal);
	
	id = getpid();
	
	write(dev,&id,4);
	count=0;
	gamble_init();
	gamble_batting();
	usleep(10);
	vkey=0x00;
	
	while(1){
		k++;
		usleep(1000);
		if(vkey==0x80)
			segflag[0]=1;
		if(vkey==0x40)
			segflag[1]=1;
		if(vkey==0x20)
			segflag[2]=1;
		if(vkey==0x10)
			segflag[3]=1;
		if(vkey==0x03)
			break;
		for(j=0;j<4;j++)
			if(segflag[j]==0)
				segval[j]=k;
		for(j=0;j<4;j++)
			segval[j]=segval[j]%100;

		segvalue=((segval[0]/10)*1000)+((segval[1])/10*100)+((segval[2])/10*10)+segval[3]/10;
		ioctl(dev,GAMBLE_PRINT,&segvalue,sizeof(int));
		if(segflag[0]==1 && segflag[1]==1 && segflag[2]==1 && segflag[3]==1){ 
			gamble_result();
			gamble_init();
			gamble_batting();
		}
	}
	close(dev);
	return 0;
}
