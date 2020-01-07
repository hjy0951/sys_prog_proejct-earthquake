#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include <bcm2835.h>
#define BCM2835_NO_DELAY_COMPATIBILITY
#undef bcm2835_delay
#undef bcm2835_delayMicroseconds
#undef delayMicroseconds

#include <wiringPi.h>
#include <wiringPiI2C.h>

#define MODE_READ 0

#define MAXLINE 1024
#define PORT 65021

// using in accel-sensor
//data format
char dtf[] = {0x31};
// register number
char xd0[] = {0x32};
char xd1[] = {0x33};
char yd0[] = {0x34};
char yd1[] = {0x35};
char zd0[] = {0x36};
char zd1[] = {0x37};

uint8_t slave_address = 0x53;

uint8_t init = 0;
uint8_t mode = 0;

// storing data
unsigned char bdf[1];//data format
unsigned char buf0[1];
unsigned char buf1[1];//x
unsigned char buf2[1];
unsigned char buf3[1];//y
unsigned char buf4[1];
unsigned char buf5[1];//z

int state;
int prev_state;
int prev_prev_state;
int msg;
int prev_msg;
short prev_x, prev_y;

//init wiringPi
void init_i2c(int sensor){
	wiringPiI2CWriteReg8(sensor, 0x2D,0x08);
	wiringPiI2CWriteReg8(sensor, 0x2E,0x80);
	wiringPiI2CWriteReg8(sensor, 0x31,0x0B);
}

void accel()
{
	bcm2835_i2c_read_register_rs(xd0,buf0,1);
	bcm2835_i2c_read_register_rs(xd1,buf1,1);
	bcm2835_i2c_read_register_rs(yd0,buf2,1);
	bcm2835_i2c_read_register_rs(yd1,buf3,1);
	bcm2835_i2c_read_register_rs(zd0,buf4,1);
	bcm2835_i2c_read_register_rs(zd1,buf5,1);

	int x0 = buf0[0];
	int x1 = buf1[0];

	int y0 = buf2[0];
	int y1 = buf3[0];

	int z0 = buf4[0];
	int z1 = buf5[0];

	short x = (x1 << 8)|x0;
	short y = (y1 << 8)|y0;
	short z = (z1 << 8)|z0;
	
	int gap_x = x - prev_x;
	if(gap_x < 0) 
		gap_x = (-1) * gap_x;

	int gap_y = y - prev_y;
	if(gap_y < 0) 
		gap_y = (-1) * gap_y;
		
	printf("\nx = %d\n",x);
	printf("y = %d\n",y);
	printf("z = %d\n\n",z);

	printf("gap_x = %d\n", gap_x);
	printf("gap_y = %d\n", gap_y);
		
	if( (40 <= gap_x && gap_x < 70) || (40 <= gap_y && gap_y < 70) ){
		printf("\n\n1111111\n\n");
		state = 1;
	}
	else if( (70 <= gap_x && gap_x < 110) || ( 70 <= gap_y && gap_y < 110) ){
		printf("\n\n2222222\n\n");
		state = 2;
	}	
	else if( (110 <= gap_x) || (110 <= gap_y) ){
		printf("\n\n3333333\n\n");
		state = 3;
	}
	else{
		state = 0;
	}

	memset(bdf,0,sizeof(unsigned char));	
	memset(buf0,0,sizeof(unsigned char));
	memset(buf1,0,sizeof(unsigned char));
	memset(buf2,0,sizeof(unsigned char));
	memset(buf3,0,sizeof(unsigned char));
	memset(buf4,0,sizeof(unsigned char));
	memset(buf5,0,sizeof(unsigned char));
	
	if(state == 1){
		msg = 1;
	}
	else if(state == 2){
		msg = 2;
	}
	else if(state == 3){
		msg = 3;
	}
	else if(state == 0){
		msg = 0;
	}
	
	prev_x = x;
	prev_y = y;
}

int main(int argc, char* argv[]){
    // using in socket
	int cli_sock;    
	struct sockaddr_in serv_addr;
	
	char buf[MAXLINE +1];
	int nbytes;

   	char* addrserv;
 	int nport;
	
	// use wiringPi - because of writing data format (bcm2835 can't access a register to write data)
	int sensor;
	wiringPiSetup();
	sensor = wiringPiI2CSetup(0x53);
	init_i2c(sensor);
	
	// use bcm2835
	// init
	printf("start bcm\n");
	if(!bcm2835_init()){
		printf("bcm2835_init failed\n");
		return 1;
	}

	if(!bcm2835_i2c_begin()){
		printf("bcm2835 i2c failed\n");
		return 1;
	}

	printf("set slave & clk div\n");
	bcm2835_i2c_setSlaveAddress(slave_address);
	bcm2835_i2c_setClockDivider(1000);
	printf("finish slave & clk div\n");
	// finish init
	
	pid_t pid; // process id

	struct hostent *host_ent;

	// init socket
	if (argc == 1){
    	nport = PORT;
	} else if (argc == 3) {
	addrserv =argv[1];
    	nport = atoi(argv[2]);
	} else {
    	printf("Usage: %s <server address>\n", argv[0]);
    	printf("   or\nUsage: %s <server address <port>>\n", argv[0]);
    	exit(0);
	}
    
	cli_sock = socket(PF_INET, SOCK_STREAM, 0);

	if (cli_sock == -1){
    	perror("socket() error!\n");
    	exit(0);
	}else{
    	printf("socket success\n" );
	}

	struct in_addr inp;
	
	inet_aton("192.168.0.30", &inp); 
	addrserv = inet_ntoa(*(struct in_addr *)&inp);

	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr(addrserv);
	serv_addr.sin_port = htons(nport);
    

	if (connect(cli_sock, (struct sockaddr*)&serv_addr,sizeof(struct sockaddr)) == -1){
    	perror("connect() error\n");
	}
	
	if((pid = fork()) == -1){//error
    	perror("fork() error\n");
    	exit(0);
	}
	else if(pid ==0) {//child
		int flag = 0;
    	while(1){
			accel();

			int start = msg;
			
			for(int i = 0 ; i <= 10 ; i++){				
				accel();
				
				if(start < msg){
					break;
				}
				if( (i == 10) && (start < msg) )
					flag = 1;
				
				delay(500);
			}
			
			if(flag == 1) {
				flag = 0;
				continue;
			}
			
			if(msg == 1){
				strcpy(buf,"1");
			}
			else if(msg == 2){
				strcpy(buf,"2");
			}
			else if(msg == 3){
				strcpy(buf,"3");
			}
			else{
				strcpy(buf,"0");
			}

			delay(500);
			nbytes = strlen(buf);
    		write(cli_sock, buf, MAXLINE);

    		if(strncmp(buf, "exit", 4) ==0) {
        		puts("exit program");
        		exit(0);
    		}
        }
	}
	else if (pid > 0){//parent
    	while(1){
        		if((nbytes = read(cli_sock, buf, MAXLINE)) < 0){
            		perror("read() error\n");
            		exit(0);
        		}
        		if(strncmp(buf, "exit",4) == 0)
            	exit(0);
    	}
	}
	
	bcm2835_i2c_end();
	bcm2835_close();

	close(cli_sock);
	return 0;
}
