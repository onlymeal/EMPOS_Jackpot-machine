#include <linux/module.h>
#include <asm/hardware.h>
#include <asm/uaccess.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/ioport.h>
#include <linux/timer.h>
#include <linux/signal.h>

#define gamble_MAJOR 			0
#define gamble_NAME 			"PUSHBUTTON_PORT"
#define gamble_MODULE_VERSION 		"PUSHBUTTON V0.1"
#define PUSHBUTTON_ADDRESS 		0xf1500000
#define PUSHBUTTON_ADDRESS_RANGE 	1
#define SEGMENT_ADDRESS_LOW		0xf1300000
#define SEGMENT_ADDRESS_HIGH		0xf1400000
#define LED_ADDRESS			0xf1600000
#define LED_ADDRESS_RANGE		1
#define	SEGMENT_ADDRESS_RANGE		4
#define TIMER_INTERVAL 20
#define GAMBLE_PRINT	0
#define GAMBLE_MONEY	1
#define GAMBLE_BAT	2
#define GAMBLE_RESULT	3
//Global variable
static int gamble_usage = 0;
static int gamble_major = 0;
static struct timer_list mytimer;
static unsigned char kvalue;
static unsigned char *addr_push;
static unsigned int *addr_seg_low;
static unsigned int *addr_seg_high;
static unsigned char *addr_led; 
static pid_t id;
static unsigned short money;
static unsigned short batting_money;
static unsigned char led_val;



// define functions...
int gamble_open(struct inode *minode, struct file *mfile) {

	if(gamble_usage != 0) return -EBUSY;
	MOD_INC_USE_COUNT;
	gamble_usage = 1;
	addr_push = (unsigned char *)PUSHBUTTON_ADDRESS;
	addr_seg_low = (unsigned int *)(SEGMENT_ADDRESS_LOW);
	addr_seg_high = (unsigned int *)(SEGMENT_ADDRESS_HIGH);
	addr_led = (unsigned char *)LED_ADDRESS;
	*addr_led=0;
	money=500;
	batting_money=0;
	led_val=0;
	return 0;
}



int gamble_release(struct inode *minode, struct file *mfile) {
	
	MOD_DEC_USE_COUNT;
	gamble_usage = 0;
	del_timer(&mytimer);
	return 0;
}

void mypollingfunction(unsigned long data) {

	kvalue = ~(*addr_push);
	if(kvalue != 0x00) kill_proc(id,SIGUSR1,1);

	mytimer.expires = jiffies + TIMER_INTERVAL;
	add_timer(&mytimer);
}

ssize_t gamble_write(struct file *inode, const char *gdata, size_t length, loff_t *off_what) {
	get_user(id,(int *)gdata);

	init_timer(&mytimer);
	mytimer.expires = jiffies + TIMER_INTERVAL;
	mytimer.function = &mypollingfunction;
	add_timer(&mytimer);
	return length;
}

unsigned int Getsegcode(int x)
{
	unsigned int code;
	switch (x) {
		case 0x0 : code = 0x3f; break;
		case 0x1 : code = 0x06; break;
		case 0x2 : code = 0x5b; break;
		case 0x3 : code = 0x4f; break;
		case 0x4 : code = 0x66; break;
		case 0x5 : code = 0x6d; break;
		case 0x6 : code = 0x7d; break;
		case 0x7 : code = 0x07; break;
		case 0x8 : code = 0x7f; break;
		case 0x9 : code = 0x6f; break;
		default : code = 0; break;
	}
	return code;
}

void ShowSegment(unsigned short value){
	unsigned int v0,v1,v2,v3;
	v0 = Getsegcode(value%10);
	v1 = Getsegcode((value%100)/10);
	v2 = Getsegcode((value%1000)/100);
	v3 = Getsegcode(value/1000);
	*addr_seg_low = (v1 << 8)|v0;
	*addr_seg_high = (v3 << 8)|v2;
}

int gamble_ioctl(struct inode *inode, struct file *file,unsigned int cmd,unsigned long gdata) {
	unsigned int segvalue, num;	
	
	switch(cmd){
		case GAMBLE_PRINT:
			copy_from_user(&segvalue,gdata,sizeof(int));
			ShowSegment(segvalue);
			kvalue=0x00;
			printk("s%d ",segvalue);
			break;
		case GAMBLE_MONEY:
			ShowSegment(money);
			printk("m%d ",money);
			break;
		case GAMBLE_BAT:
			batting_money+=50;
			money-=50;
			if(batting_money==50)
				led_val=0x01;
			else
				led_val= (led_val<<1)| 0x01;
			*addr_led=led_val;
			break;	
		case GAMBLE_RESULT:
			copy_from_user(&num,gdata,sizeof(int));	
			if(num==1)
				money=money+(batting_money*2);
			else if(num==2)
				money=money+(batting_money*4);
			else if(num==4)
				money=money+(batting_money*10);
			batting_money=0;
			ShowSegment(money);
			printk("n%d ",num);
			break;	
		default:
			printk("driver : no such command!\n");
			return -ENOTTY;
	}
	return 0;
}	
	
ssize_t gamble_read(struct file *inode, char *gdata, size_t length, loff_t *off_what) {
	
	copy_to_user(gdata,&kvalue,1);
	return length;
}

struct file_operations gamble_fops = {
	read: gamble_read,
	write: gamble_write,
	ioctl: gamble_ioctl,
	open: gamble_open,
	release: gamble_release,
};

int init_module(void) {
	int result;
	result = register_chrdev(gamble_MAJOR,gamble_NAME,&gamble_fops);
	if(result < 0) {
		printk(KERN_WARNING"Can't get any major\n");
		return result;
	}
	gamble_major = result;
	if(!check_region(PUSHBUTTON_ADDRESS,PUSHBUTTON_ADDRESS_RANGE) || !check_region(SEGMENT_ADDRESS_LOW, SEGMENT_ADDRESS_RANGE) || !check_region(SEGMENT_ADDRESS_HIGH, SEGMENT_ADDRESS_RANGE) || !check_region(LED_ADDRESS, LED_ADDRESS_RANGE)){
		request_region(PUSHBUTTON_ADDRESS,PUSHBUTTON_ADDRESS_RANGE,gamble_NAME);
		request_region(SEGMENT_ADDRESS_LOW,SEGMENT_ADDRESS_RANGE,gamble_NAME);
		request_region(SEGMENT_ADDRESS_HIGH, SEGMENT_ADDRESS_RANGE,gamble_NAME);
		request_region(LED_ADDRESS, LED_ADDRESS_RANGE,gamble_NAME);
	}
	else printk("driver : unable to register this!\n");
	printk("init module, gamble major number : %d\n",result);
	return 0;
}

void cleanup_module(void) {
	release_region(PUSHBUTTON_ADDRESS,PUSHBUTTON_ADDRESS_RANGE);
	release_region(SEGMENT_ADDRESS_LOW,SEGMENT_ADDRESS_RANGE);
	release_region(SEGMENT_ADDRESS_HIGH,SEGMENT_ADDRESS_RANGE);
	release_region(LED_ADDRESS,LED_ADDRESS_RANGE);
	
	if(unregister_chrdev(gamble_major,gamble_NAME))
		printk("driver : %s DRIVER CLEANUP FALLED\n",gamble_NAME);
}
