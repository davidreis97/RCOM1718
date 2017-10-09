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

const char FLAG = 0x7E;

const char SENDER_ADDRESS = 0x03;
const char RECEIVER_ADDRESS = 0x01;

const char CTRL_SET = 0b00000011;
const char CTRL_DISC = 0b00001011;
const char CTRL_UA = 0b00000111;
const char CTRL_RR = 0b00000101;
const char CTRL_REJ = 0b0000001;

const int BUFFER_SIZE = 255;

void *run(void *(*f)(int, void *), void *args, char *port)
{
    int fd;
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

    void * retVal = f(fd,args);
    
    tcflush(fd,TCIOFLUSH);
    tcsetattr(fd,TCSANOW,&oldtio);
    close(fd);
    return retVal;
}

void *createAndSend(int fd, void *args){
    char buffer[BUFFER_SIZE];
    bzero(buffer, 255);
    
    buffer[0] = FLAG;
	buffer[1] = SENDER_ADDRESS;
	buffer[2] = CTRL_SET;
	buffer[3] = 0; //Block Check Character (BCC)
	buffer[4] = FLAG;
    buffer[5] = '\0';
    
    int num = write(fd,buffer,5);
    printf("Wrote %d bytes\n",num);
    return NULL;
}

int main(){
    run(createAndSend,NULL,"/dev/ttyS0");
}

