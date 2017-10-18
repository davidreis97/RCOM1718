#include "linklayer.h"

const char C_DATA = 1;
const char C_START = 2;
const char C_END = 3; 

const char T_SIZE = 0;
const char T_NAME = 1;

const char S_SIZE = 4; 
const unsigned char S_NAME = 255;

const int APP_BUFFER_SIZE = 65525; //2^16 - 10 bits for link layer

typedef struct appLayer {
    int fd;
    char *filename;
    int expectedSize;
}APP_LAYER;

APP_LAYER al;

void setFileSize(char *buffer){
	al.fd = open(al.filename,O_RDONLY);
    struct stat buf;
	fstat(al.fd, &buf);
	al.expectedSize = buf.st_size;

	char * size_c = (char *) &al.expectedSize; //Big Endian Switch
	buffer[0] = size_c[3];
	buffer[1] = size_c[2];
	buffer[2] = size_c[1];
	buffer[3] = size_c[0];
}

void setFileName(char *buffer){
	memcpy(buffer, al.filename, strlen(al.filename));
	bzero(buffer + strlen(al.filename),S_NAME - strlen(al.filename));
}

void getFileSize(){
	printf("Getting file size from buffer\n");
}

void getFileName(){
	printf("Getting file name from buffer\n");
}

int sendStart(){
	char buffer[APP_BUFFER_SIZE];

	buffer[0] = C_START;
	buffer[1] = T_SIZE;
	buffer[2] = S_SIZE;
	setFileSize(buffer + 3);
	buffer[3 + S_SIZE] = T_NAME;
	buffer[4 + S_SIZE] = S_NAME;
	setFileName(buffer + 5 + S_SIZE);

	return llwrite(buffer,5 + S_SIZE + S_NAME);
}

int receiveStart(){
	char buffer[APP_BUFFER_SIZE];
	bzero(buffer,APP_BUFFER_SIZE);
	int c = llread(buffer);
	int i = 0;
	
	if(buffer[0] != C_START){
		printf("Received Wrong Packet - Not Start\n");
		return -1;
	}else if(c < 0){
		printf("llread returned error\n");
		return -1;
	}

	printBuffer(buffer,c,"");

	for(i = 1; i < c; ){
		printf("i: %d ; buffer[i]: %x\n", i, buffer[i]);
		if(buffer[i] == T_SIZE){
			getFileSize();
		}else if(buffer[i] == T_NAME){
			getFileName();
		}else{
			printf("Unknown type: %x Skipping it\n",buffer[i]);
		}

		i++;
		i += (unsigned char) buffer[i];
		i++;
	}

}

int main(int argc, char*argv[]){
    if (argc >= 2){
        if(!strcmp(argv[1], "--debug")){
            setDebug(1);        
        }else{
            printf("Usage: %s [--debug]\n",argv[0]);  
            return -1;     
        }
    }else{
        setDebug(0);    
    }

    LINK_LAYER ll;
	ll.port = "/dev/ttyS0";
	ll.baudRate = B38400;
	ll.sequenceNumber = PAIR;
	ll.timeout = 3;
	ll.numTransmissions = 3;
	ll.status = RECEIVER;
	setLL(ll);
    	
	al.filename = "linklayer.c";

	if(ll.status == TRANSMITTER){
		llopen();
		sendStart();
		llclose();
	}else if(ll.status == RECEIVER) {
		llopen();
		receiveStart();
	}
	
}