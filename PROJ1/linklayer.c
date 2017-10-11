#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <signal.h>
#include <unistd.h>

#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define TRANSMITTER 0
#define RECEIVER 1

const char FLAG = 0x7E;

const char SENDER_ADDRESS = 0x03;
const char RECEIVER_ADDRESS = 0x01;

const char CTRL_SET = 0b00000011;
const char CTRL_DISC = 0b00001011;
const char CTRL_UA = 0b00000111;
const char CTRL_RR = 0b00000101;
const char CTRL_REJ = 0b0000001;

const int BUFFER_SIZE = 255;

typedef struct applicationLayer {
	int fd;
	int status;
}APPLICATION_LAYER;

typedef struct linkLayer {
	char *port;
	int baudRate;
	unsigned int sequenceNumber;
	unsigned int timeout;
	unsigned int numTransmissions;
	struct termios oldtio;
}LINK_LAYER;

LINK_LAYER ll;
APPLICATION_LAYER al;

int TIMEOUT_APPLIED = 0; int TIMEOUT_TRIES;


void changeBlocking(int block){
	int flags = fcntl(al.fd, F_GETFL, 0);
	if (block){
		fcntl(al.fd, F_SETFL, flags & ~O_NONBLOCK);
	}else {
		fcntl(al.fd, F_SETFL, flags | O_NONBLOCK);
	}
}



void atende() {
	printf("alarme # %d\n", TIMEOUT_TRIES);
	TIMEOUT_APPLIED = 0;
	TIMEOUT_TRIES--;

	changeBlocking(0);
}

int llwrite(){

}

int llclose(){
	int ch = 0, n = 0;
	char buffer[BUFFER_SIZE];
	bzero(buffer, BUFFER_SIZE);

    if(al.status == TRANSMITTER){
		TIMEOUT_TRIES = ll.numTransmissions;

		while(TIMEOUT_TRIES){
			signal(SIGALRM, atende);
            
		    if(!TIMEOUT_APPLIED){  
				alarm(ll.timeout);
				TIMEOUT_APPLIED=1;

                buffer[0] = FLAG;
		        buffer[1] = SENDER_ADDRESS;
		        buffer[2] = CTRL_DISC;
		        buffer[3] = 0; //Block Check Character (BCC)
		        buffer[4] = FLAG;
		        buffer[5] = '\0';

		        int num = write(al.fd,buffer,5);

                printf("Sent %d bytes and applied timeout\n",num);
			}

			changeBlocking(1);

			if((ch = read(al.fd,buffer+n,1)) <=0 ){
            	//Do nothing        
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
        		break;
        	}
        	if (ch != 0){
        		n++;
        	}
    	}

    	if(buffer[2] == CTRL_DISC){
    		printf("Successfully closed! Sending UA\n");
            
            buffer[0] = FLAG;
		    buffer[1] = SENDER_ADDRESS;
		    buffer[2] = CTRL_UA;
		    buffer[3] = 0; //Block Check Character (BCC)
		    buffer[4] = FLAG;
		    buffer[5] = '\0';

		    int num = write(al.fd,buffer,5);
    	}
	}else if(al.status == RECEIVER){
		TIMEOUT_TRIES = ll.numTransmissions;

		while(TIMEOUT_TRIES){
			signal(SIGALRM, atende);
            
		    if(!TIMEOUT_APPLIED){  
				alarm(ll.timeout);
				TIMEOUT_APPLIED=1;

                buffer[0] = FLAG;
		        buffer[1] = SENDER_ADDRESS;
		        buffer[2] = CTRL_DISC;
		        buffer[3] = 0; //Block Check Character (BCC)
		        buffer[4] = FLAG;
		        buffer[5] = '\0';

		        int num = write(al.fd,buffer,5);

                printf("Sent %d bytes and applied timeout\n",num);
			}

			changeBlocking(1);

			if((ch = read(al.fd,buffer+n,1)) <=0 ){
            	//Do nothing        
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
        		break;
        	}
        	if (ch != 0){
        		n++;
        	}
    	}

    	if(buffer[2] == CTRL_UA){
    		printf("Successfully closed!\n");
    	}
	}
    
	tcflush(al.fd,TCIOFLUSH);
    tcsetattr(al.fd,TCSANOW,&(ll.oldtio));
    close(al.fd);
    return 0;	
}

int llopen(){
	struct termios newtio;
	
	al.fd = open(ll.port, O_RDWR | O_NOCTTY);
	if (al.fd <0) {perror(ll.port); exit(-1); }

	if ( tcgetattr(al.fd,&(ll.oldtio)) == -1) {
		perror("tcgetattr");
		exit(-1);
	}

	bzero(&newtio, sizeof(newtio));
	newtio.c_cflag = ll.baudRate | CS8 | CLOCAL | CREAD;
	newtio.c_iflag = IGNPAR;
	newtio.c_oflag = 0;

	newtio.c_lflag = 0;

	newtio.c_cc[VTIME]    = 0;  
	newtio.c_cc[VMIN]     = 1;

	tcflush(al.fd, TCIOFLUSH);

	if ( tcsetattr(al.fd,TCSANOW,&newtio) == -1) {
		perror("tcsetattr");
		exit(-1);
	}

	int ch = 0, n = 0;
	char buffer[BUFFER_SIZE];
	bzero(buffer, BUFFER_SIZE);
		
	if(al.status == TRANSMITTER){
		TIMEOUT_TRIES = ll.numTransmissions;

		while(TIMEOUT_TRIES){
			signal(SIGALRM, atende);
            
		    if(!TIMEOUT_APPLIED){  
				alarm(ll.timeout);
				TIMEOUT_APPLIED=1;

                buffer[0] = FLAG;
		        buffer[1] = SENDER_ADDRESS;
		        buffer[2] = CTRL_SET;
		        buffer[3] = 0; //Block Check Character (BCC)
		        buffer[4] = FLAG;
		        buffer[5] = '\0';

		        int num = write(al.fd,buffer,5);

                printf("Sent %d bytes and applied timeout\n",num);
			}

			changeBlocking(1);

			if((ch = read(al.fd,buffer+n,1)) <=0 ){
            	//Do nothing        
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
        		break;
        	}
        	if (ch != 0){
        		n++;
        	}
    	}

    	if(buffer[2] == CTRL_UA){
    		printf("Success!");
    	}
	}else if(al.status == RECEIVER){
		while(1){
		    if((ch = read(al.fd,buffer+n,1)) <=0 ){
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
		        break;
		    }
		    n++;
		}

		if(buffer[2] == CTRL_SET){
			buffer[0] = FLAG;
			buffer[1] = RECEIVER_ADDRESS;
			buffer[2] = CTRL_UA;
			buffer[3] = 0; //Block Check Character (BCC)
			buffer[4] = FLAG;
			buffer[5] = '\0';

			int num = write(al.fd,buffer,5);
			printf("Wrote %d bytes\n",num);
		}
	}

	return 0;
}

int main(){
	ll.port = "/dev/ttyS0";
	ll.baudRate = B38400;
	ll.sequenceNumber = 0; //?????????
	ll.timeout = 3;
	ll.numTransmissions = 3;

	al.status = RECEIVER;
	llopen();
}

