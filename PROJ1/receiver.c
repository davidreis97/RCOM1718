/*Non-Canonical Input Processing*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>

#define BAUDRATE B38400
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1

volatile int STOP=FALSE;

const char FLAG = 0x7E;

const char RECEIVER_ADRESS = 0x01;
const char SENDER_ADRESS = 0x03;

const char CONTROL_SET = 0b00000011;
const char CONTROL_DISC = 0b00001011;
const char CONTROL_UA = 0b0000111;
const char CONTROL_RR = 0b000101;
const char CONTROL_REJ = 0b00001;

int process_frame( unsigned char* buf, int n){

	if (buf[2] == CONTROL_SET){
		buf[2] = CONTROL_UA;
		printf("CHANGED \n ");
		}

			
}

int frame_reader(int fd, unsigned char* buf, int maxlen){

	int n=0;
	int ch;
	
	while(1){
		if(ch=read(fd,buf+1,1))<=0)
			return ch; // error;
		if(n==0 && buf[n]!=FLAG)
			continue;
		if(n==1 && buf[n]=FLAG)
			continue;
		n++;
		if(buf[n-1] != FLAG && n==maxlen){
			n=0;
			continue;
		}
		if(buf[n-1]== FLAG && n>2){
		process_frame(buf,n);
		return n;
	}


}


void receiver(char* serialPort){

int fd,c, res;
    struct termios oldtio,newtio;
    char buf[255];


  /*
    Open serial port device for reading and writing and not as controlling tty
    because we don't want to get killed if linenoise sends CTRL-C.
  */
  
    
    fd = open(serialPort, O_RDWR | O_NOCTTY );
    if (fd <0) {perror(serialPort); exit(-1); }

    if ( tcgetattr(fd,&oldtio) == -1) { /* save current port settings */
      perror("tcgetattr");
      exit(-1);
    }

    bzero(&newtio, sizeof(newtio));
    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    /* set input mode (non-canonical, no echo,...) */
    newtio.c_lflag = 0;

    newtio.c_cc[VTIME]    = 1;   /* inter-character timer unused */
    newtio.c_cc[VMIN]     = 1;   /* blocking read until 5 chars received */



  /* 
    VTIME e VMIN devem ser alterados de forma a proteger com um temporizador a 
    leitura do(s) próximo(s) caracter(es)
  */



    tcflush(fd, TCIOFLUSH);

    if ( tcsetattr(fd,TCSANOW,&newtio) == -1) {
      perror("tcsetattr");
      exit(-1);
    }

    printf("New termios structure set\n");

	
    frame_reader(fd,buf,255);
    
    



  /* 
    O ciclo WHILE deve ser alterado de modo a 
    
    respeitar o indicado no guião 
  */



    tcsetattr(fd,TCSANOW,&oldtio);
    close(fd);
    return 0;
}

 
int main(int argc, char** argv)
{
    if ( (argc < 2) || 
  	     ((strcmp("/dev/ttyS0", argv[1])!=0) && 
  	      (strcmp("/dev/ttyS1", argv[1])!=0) )) {
      printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS1\n");
      exit(1);
    }
    receiver(argv[1]);

    
    
}

