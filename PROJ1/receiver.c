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

void * receiveAndProcess(int fd, void *args){
    char *buffer = (char *) args;
    int n = 0;
    int ch;
    while(1){
        if((ch = read(fd,buffer+n,1)) <=0 ){
            return ch; //Error case        
        }else if (n==0 && buffer[0] != FLAG){
            printf("No flag on initial byte.\n");
            continue;
        }else if (n==1 && buffer[1] == FLAG){
            printf("Flag after first byte..\n");
            continue;
        }else if (buffer[n] != FLAG && n == BUFFER_SIZE - 1){ //Wraps around?...
            printf("Exceeded buffer.\n");
            n = 0;
            continue;
        }else if (buffer[n] == FLAG && n > 2){
            processFrame(buffer,n+1);
            return NULL;
        }
        n++;
        printf("Finished loop peacefully.\n");
    }
}

void processFrame(char *buffer, int n){
    printf("Received a frame with %d bytes\n",n);
}

int main(){
    char buffer[BUFFER_SIZE];
    run(receiveAndProcess,buffer,"/dev/ttyS0");
}

