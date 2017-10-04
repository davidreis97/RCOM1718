#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>

#define BAUDRATE B38400

const char FLAG = 0x7E;

const char SENDER_ADDRESS = 0x03;
const char RECEIVER_ADDRESS = 0x01;

const char CTRL_SET = 0b00000011;
const char CTRL_DISC = 0b00001011;
const char CTRL_UA = 0b00000111;
const char CTRL_RR = 0b00000101;
const char CTRL_REJ = 0b0000001;

const int BUFFER_SIZE = 255;

typedef enum status{STARTED, STOPPED, ENDED}STATUS;

unsigned int sendSET(char *port){
	char buffer[BUFFER_SIZE];
	bzero(buffer, 255);
	buffer[0] = FLAG;
	buffer[1] = SENDER_ADDRESS;
	buffer[2] = CTRL_SET;
	buffer[3] = 0; //Block Check Character (BCC)
	buffer[4] = FLAG;
	buffer[5] = '\0';

	int fd,c, res;
	struct termios oldtio,newtio;
	int i, sum = 0, speed = 0;

	fd = open(port, O_RDWR | O_NOCTTY );
	if (fd <0) {perror(port); exit(-1); }

	if ( tcgetattr(fd,&oldtio) == -1) {
		perror("tcgetattr");
		exit(-1);
	}

	bzero(&newtio, sizeof(newtio));
	newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
	newtio.c_iflag = IGNPAR;
	newtio.c_oflag = 0;

	newtio.c_lflag = 0;

	newtio.c_cc[VTIME]    = 2;   
	newtio.c_cc[VMIN]     = 10;

	tcflush(fd, TCIOFLUSH);

	if ( tcsetattr(fd,TCSANOW,&newtio) == -1) {
		perror("tcsetattr");
		exit(-1);
	}

	printf("New termios structure set\n");

	res = write(fd,buffer,255);   
	printf("%d bytes written\n", res);

	if ( tcsetattr(fd,TCSANOW,&oldtio) == -1) {
		perror("tcsetattr");
		exit(-1);
	}

	close(fd);
	return 0;
}

char *receive(char buffer[],char port[]){
	int fd,c, res;
	struct termios oldtio,newtio;

	fd = open(port, O_RDWR | O_NOCTTY );
	if (fd <0) {perror(port); exit(-1); }

	if ( tcgetattr(fd,&oldtio) == -1) { 
		perror("tcgetattr");
		exit(-1);
	}

	bzero(&newtio, sizeof(newtio));
	newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
	newtio.c_iflag = IGNPAR;
	newtio.c_oflag = 0;

	newtio.c_lflag = 0;

	newtio.c_cc[VTIME]    = 0;    
	newtio.c_cc[VMIN]     = 1;   

	tcflush(fd, TCIOFLUSH);

	if ( tcsetattr(fd,TCSANOW,&newtio) == -1) {
		perror("tcsetattr");
		exit(-1);
	}

	printf("New termios structure set\n");

	int pos = 0;
	STATUS st = STOPPED;

	do {
		char received;
		int size = read(fd,&received,1);   

		if(size == 1){
			if(res == 0x7E){
				if(st == STOPPED){
					st = STARTED;
				}else{
					st = ENDED;
				}
			}
			buffer[pos++] = received;
		}
	}while (st != ENDED);

	buffer[pos++] = 0;

	printf("0x%08x\n", buffer);

	tcsetattr(fd,TCSANOW,&oldtio);
	close(fd);
	return buffer;
}

int main(int argc, char *argv[]){
	if ( (argc < 2) || 
		((strcmp("/dev/ttyS0", argv[1])!=0) && 
			(strcmp("/dev/ttyS1", argv[1])!=0) )) {
		printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS1\n");
	exit(1);
	}

	char buffer[BUFFER_SIZE];
	receive(buffer, argv[1]);

	return 0;
}