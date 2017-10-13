#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <signal.h>
#include <unistd.h>

#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define TRANSMITTER 0
#define RECEIVER 1
#define PAIR 0
#define ODD 1

const char FLAG = 0x7E;

const char SENDER_ADDRESS = 0x03;
const char RECEIVER_ADDRESS = 0x01;

const char CTRL_SET = 0b00000011;
const char CTRL_DISC = 0b00001011;
const char CTRL_CTRL[] = {0b00000000,0b01000000};
const char CTRL_UA = 0b00000111;
const char CTRL_RR[] = {0b00000101,0b10000101};
const char CTRL_REJ[] = {0b00000001,0b10000001};

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

void parseAlarm() {
	printf("PARSEALARM - Received alarm #%d\n", TIMEOUT_TRIES);
	TIMEOUT_APPLIED = 0;
	TIMEOUT_TRIES--;

	changeBlocking(0);
}

int timeoutAndSend(char *buffer, unsigned int size){
	printf("TIMEOUTANDSEND - Entering\n");
	int n = 0, ch=0, bytesWritten = 0, success = 0;

	TIMEOUT_TRIES = ll.numTransmissions;

	while(TIMEOUT_TRIES){
		signal(SIGALRM, parseAlarm);
        
	    if(!TIMEOUT_APPLIED){  
			alarm(ll.timeout);
			TIMEOUT_APPLIED=1;

	       	//byteStuffing(buffer); TODO

	        bytesWritten = write(al.fd,buffer,size);

            printf("TIMEOUTANDSEND - Sent %d bytes and applied timeout to %ds\n",bytesWritten, ll.timeout);
		}

		changeBlocking(1);
    
		if((ch = read(al.fd,buffer+n,1)) <=0 ){
        	//Do nothing        
    	}else if (n==0 && buffer[0] != FLAG){
    		printf("TIMEOUTANDSEND - No flag on initial byte.\n");
    		continue;
    	}else if (n==1 && buffer[1] == FLAG){
    		printf("TIMEOUTANDSEND - Flag after first byte.\n");
    		continue;
    	}else if (buffer[n] != FLAG && n == BUFFER_SIZE - 1){
    		printf("TIMEOUTANDSEND - Exceeded buffer.\n");
    		n = 0;
    		continue;
    	}else if (buffer[n] == FLAG && n > 2){
    		buffer[n+1] = '\0';
    		success = 1;
    		break;
    	}
    	if (ch != 0){
			alarm(0); //Cancels pending alarms
            printf("TIMEOUTANDSEND - Received buffer[%d]: %02X\n",n,buffer[n]);    
    		n++;
    	}
	}

	printf("TIMEOUTANDSEND - Canceling all alarms\n");
	alarm(0); //Cancels pending alarms
    TIMEOUT_APPLIED = 0;

	if(!success){
		bzero(buffer,255);
	}

    printf("TIMEOUTANDSEND - Leaving. Received %d bytes.\n",n);
    return bytesWritten;
}

int llwrite(char *packet, unsigned int size){ //Important: Must be '\0' terminated for strcat and timeoutAndSend
	printf("LLWRITE - Entering\n");
	int ch = 0, bytesWritten;
	char buffer[BUFFER_SIZE];
	bzero(buffer, BUFFER_SIZE);

	(ll.sequenceNumber == PAIR) ? (ll.sequenceNumber = ODD) : (ll.sequenceNumber = PAIR); //Switches sequenceNumber
		
    buffer[0] = FLAG;
    buffer[1] = SENDER_ADDRESS;
    buffer[2] = CTRL_CTRL[ll.sequenceNumber];
    buffer[3] = 0; //Block Check Character 1(BCC1)
    buffer[4] = '\0';
    strcat(buffer,packet); // <- NEEDS STUFFING
    int i = strlen(buffer);
   	buffer[i] = 0; //Block Check Character 2(BCC2) <- NEEDS STUFFING
    buffer[i+1] = FLAG;
	buffer[i+2] = '\0';

	printf("LLWRITE - Sending and Waiting\n");
    bytesWritten = timeoutAndSend(buffer,i+2);

	if(buffer[2] == CTRL_RR[ll.sequenceNumber]){ //TODO - Check BCC...
		if(1/*!checkControlBCC(buffer,2)*/){
			printf("LLWRITE - Successfully sent trace! Exiting function\n");
			printf("LLWRITE - Leaving\n");
			return bytesWritten;
		}else{
			printf("LLWRITE - Error checking CTRL_BCC 1\n");
		}
	}else if(buffer[2] == CTRL_REJ[ll.sequenceNumber]){
		printf("LLWRITE - Rejected: %02X , Current Parity: %d\n", buffer[2], ll.sequenceNumber);
	}else{
    	printf("LLWRITE - Failed to write on llwrite: Expected (%02X) but got (%02X)\n",CTRL_RR[ll.sequenceNumber], buffer[2]);
	}
	printf("LLWRITE - Leaving with error\n");
	return 1;
}

int llclose(){
	printf("LLCLOSE - Entering\n");
	int ch = 0, n = 0;
	char buffer[BUFFER_SIZE];
	bzero(buffer, BUFFER_SIZE);

	(ll.sequenceNumber == PAIR) ? (ll.sequenceNumber = ODD) : (ll.sequenceNumber = PAIR); //Switches sequenceNumber

    if(al.status == TRANSMITTER){
		buffer[0] = FLAG;
		buffer[1] = SENDER_ADDRESS;
		buffer[2] = CTRL_DISC;
		buffer[3] = 0; //Block Check Character (BCC)
		buffer[4] = FLAG;
		buffer[5] = '\0';

		printf("LLCLOSE - Sending and waiting\n");
		timeoutAndSend(buffer,5);

    	if(buffer[2] == CTRL_DISC){
    		printf("LLCLOSE - Successfully received CTRL_DISC! Sending UA\n");
            
            buffer[0] = FLAG;
		    buffer[1] = SENDER_ADDRESS;
		    buffer[2] = CTRL_UA;
		    buffer[3] = 0; //Block Check Character (BCC)
		    buffer[4] = FLAG;
		    buffer[5] = '\0';

		    int num = write(al.fd,buffer,6);
    	}else{
    		printf("LLCLOSE - Failed to close on llclose: Expected (%02X) but got (%02X)\n",CTRL_DISC, buffer[2]);
    	}
	}else if(al.status == RECEIVER){
		buffer[0] = FLAG;
		buffer[1] = SENDER_ADDRESS;
		buffer[2] = CTRL_DISC;
		buffer[3] = 0; //Block Check Character (BCC)
		buffer[4] = FLAG;
		buffer[5] = '\0';

		timeoutAndSend(buffer,5);

    	if(buffer[2] == CTRL_UA){
    		printf("LLCLOSE - Received UA on llclose: Successfully closed!\n");
    	}else{
    		printf("LLCLOSE - Failed to close on llclose: Expected (%02X) but got (%02X)\n",CTRL_UA, buffer[2]);
    	}
	}
    
	tcflush(al.fd,TCIOFLUSH);
    tcsetattr(al.fd,TCSANOW,&(ll.oldtio));
    close(al.fd);
    printf("LLCLOSE - Leaving\n");
    return 0;	
}

int llread(char *buffer){
	printf("LLREAD - Entering\n");
	int ch = 0, n = 0;

	(ll.sequenceNumber == PAIR) ? (ll.sequenceNumber = ODD) : (ll.sequenceNumber = PAIR); //Switches sequenceNumber

	while(1){
	    if((ch = read(al.fd,buffer+n,1)) <=0 ){

	    }else if (n==0 && buffer[0] != FLAG){
	        printf("LLREAD - No flag on initial byte (%02X) \n", buffer[0]);
	        continue;
	    }else if (n==1 && buffer[1] == FLAG){
	        printf("LLREAD - Flag after first byte..\n");
	        continue;
	    }else if (buffer[n] != FLAG && n == BUFFER_SIZE - 1){
	        printf("LLREAD - Exceeded buffer.\n");
	        n = 0;
	        continue;
	    }else if (buffer[n] == FLAG && n > 2){
	    	buffer[n+1] = '\0';
	        break;
	    }

	    if (ch != 0){
        	n++;
        }
	}

	printf("LLREAD - Received: %s \n",buffer);

	//byteDestuffing(buffer);

	char retBuffer[BUFFER_SIZE];
	bzero(retBuffer, BUFFER_SIZE);

	if(buffer[2] == CTRL_CTRL[ll.sequenceNumber] /*&& checkDataBCC() && checkControlBCC()*/ ){
		retBuffer[0] = FLAG;
		retBuffer[1] = SENDER_ADDRESS;
		retBuffer[2] = CTRL_RR[ll.sequenceNumber];
		retBuffer[3] = 0; //Block Check Character (BCC)
		retBuffer[4] = FLAG;
		retBuffer[5] = '\0';

		int num = write(al.fd,retBuffer,6);
		printf("LLREAD - CTRL_CTRL received, CTRL_RR sent\n");
	}else if(buffer[2] == CTRL_DISC){ 
		printf("LLREAD - CTRL_DISC received, calling llclose()\n");
		llclose();
	}else{
		retBuffer[0] = FLAG;
		retBuffer[1] = SENDER_ADDRESS;
		retBuffer[2] = CTRL_REJ[ll.sequenceNumber];
		retBuffer[3] = 0; //Block Check Character (BCC)
		retBuffer[4] = FLAG;
		retBuffer[5] = '\0';

		int num = write(al.fd,buffer,6);
		printf("LLREAD - CTRL_REJ sent, received %02X expected %02X\n", buffer[2], CTRL_CTRL[ll.sequenceNumber]);
	}

	printf("LLREAD - Leaving\n");
	return ch;
}

int llopen(){
	printf("LLOPEN - Entering\n");
	
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
        buffer[0] = FLAG;
        buffer[1] = SENDER_ADDRESS;
        buffer[2] = CTRL_SET;
        buffer[3] = 0; //Block Check Character (BCC)
        buffer[4] = FLAG;
        buffer[5] = '\0';

        printf("LLOPEN - Sending CTRL_SET and waiting CTRL_UA\n");
		timeoutAndSend(buffer,5);        
 		
    	if(buffer[2] == CTRL_UA){
    		printf("LLOPEN - Successfully received CTRL_UA\n");
    	}else{
    		printf("LLOPEN - Failed to open: Expected (%02X) but got (%02X)\n",CTRL_UA, buffer[2]);
    	}
	}else if(al.status == RECEIVER){
		while(1){
		    if((ch = read(al.fd,buffer+n,1)) <=0 ){
		        return ch; //Error case        
		    }else if (n==0 && buffer[0] != FLAG){
		        printf("LLOPEN - No flag on initial byte.\n");
		        continue;
		    }else if (n==1 && buffer[1] == FLAG){
		        printf("LLOPEN - Flag after first byte..\n");
		        continue;
		    }else if (buffer[n] != FLAG && n == BUFFER_SIZE - 1){ //Wraps around?...
		        printf("LLOPEN - Exceeded buffer.\n");
		        n = 0;
		        continue;
		    }else if (buffer[n] == FLAG && n > 2){
		        break;
		    }
		    if (ch != 0){
        		n++;
        	}
		}

		if(buffer[2] == CTRL_SET){
			buffer[0] = FLAG;
			buffer[1] = SENDER_ADDRESS;
			buffer[2] = CTRL_UA;
			buffer[3] = 0; //Block Check Character (BCC)
			buffer[4] = FLAG;
			buffer[5] = '\0';

			int num = write(al.fd,buffer,5);
			printf("LLOPEN - Sent CTRL_SET\n");
		}else{
    		printf("LLOPEN - Failed to open: Expected (%02X) but got (%02X)\n",CTRL_UA, buffer[2]);
		}
	}

	printf("LLOPEN - Leaving\n");

	return 0;
}

void testTransmitter(){
	al.status = TRANSMITTER;
	llopen();
	llwrite("teste\0",6);
	llclose();
}

void testReceiver(){
	char buffer[255];
	bzero(buffer,255);

	al.status = RECEIVER;
	llopen();
	int c = llread(buffer);
	bzero(buffer,255);
	llread(buffer);
}

int main(int argc,char *argv[]){
	ll.port = "/dev/ttyS0";
	ll.baudRate = B38400;
	ll.sequenceNumber = 0;
	ll.timeout = 3;
	ll.numTransmissions = 3;

	
}

