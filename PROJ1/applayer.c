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
    char filename[256];
    int expectedSize;
    unsigned char sequenceNumber;
}APP_LAYER;

APP_LAYER al;

void setFileSize(char *buffer){
	al.fd = open(al.filename,O_RDONLY);
    struct stat buf;
	fstat(al.fd, &buf);
	al.expectedSize = buf.st_size;

	buffer[0] = ((unsigned int)al.expectedSize >> (3 << 3)) & 0xff; //Assures Big Endianness - works on both systems
	buffer[1] = ((unsigned int)al.expectedSize >> (2 << 3)) & 0xff;
	buffer[2] = ((unsigned int)al.expectedSize >> (1 << 3)) & 0xff;
	buffer[3] = ((unsigned int)al.expectedSize >> (0 << 3)) & 0xff;
}

void setFileName(char *buffer){
	memcpy(buffer, al.filename, strlen(al.filename));
	bzero(buffer + strlen(al.filename),S_NAME - strlen(al.filename));
}

void getFileSize(char *buffer){
	al.expectedSize = 256*256*256*buffer[0] +
					  256*256*buffer[1] +
					  256*buffer[2] +
					  buffer[3];
}

void getFileName(char *buffer){
	memcpy(al.filename,buffer,S_NAME);
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

	for(i = 1; i < c; ){
		printf("i: %d ; buffer[i]: %x\n", i, buffer[i]);
		if(buffer[i] == T_SIZE){
			getFileSize(buffer + i + 2);
		}else if(buffer[i] == T_NAME){
			getFileName(buffer + i + 2);
		}else{
			printf("Unknown type: %x Skipping it\n",buffer[i]);
		}

		i++;
		i += (unsigned char) buffer[i];
		i++;
	}

	printf("Found file called %s with a size of %d bytes\n",al.filename,al.expectedSize);
}

int receiveData(){
	char buffer[APP_BUFFER_SIZE];
	bzero(buffer,APP_BUFFER_SIZE);
	int bRead = 0, writeFd;

	if ((writeFd = open(al.filename,O_WRONLY | O_APPEND | O_CREAT | O_EXCL)) <= 0){ //Currently creating file with wrong permissions.
		printf("Error: Could not open file [%s] for write. (File already exists?)\n",al.filename);
	}

	do{
		bzero(buffer,APP_BUFFER_SIZE);
		bRead = llread(buffer);


		if(bRead < 5 && bRead != 0){
			printf("Error: Packet received was too short (%d bytes)\n",bRead);
		}else if(buffer[0] != C_DATA){
			printf("Error: Received non data packet\n");
		}else if(buffer[1] != al.sequenceNumber++){
			printf("Error: Packets desynchronized expected (%d) got (%d)\n",al.sequenceNumber,buffer[1]);
		}else if(bRead - 4 != (256*(unsigned char) buffer[2]) + (unsigned char) buffer[3]){
			printf("Error: Packet has wrong size expected (%d) got (%d) \n",
				   bRead - 4, (256*(unsigned char) buffer[2]) + (unsigned char) buffer[3]);
		}
		
		if(bRead != 0) {
			if (write(writeFd, buffer + 4, bRead - 4) != bRead - 4){
				printf("Could not write to file.\n");
			}
		}

	}while(bRead > 0);

	//TODO - check final file size with al.expectedSize
}

int sendData(){
	int packetSize = 16, bRead = 0;
	char buffer[packetSize]; //Fixed buffer size. Change? Random?

	//Fix - Does not need to send the full packet size on the last packet

	do{
		buffer[0] = C_DATA;
		buffer[1] = al.sequenceNumber++; //Let it overflow, let it overflow, cant hold it back anymore
		bzero(buffer+4,packetSize-4);
		bRead = read(al.fd, buffer+4, packetSize-4);
		buffer[2] = ((unsigned short)bRead >> (1 << 3)) & 0xff;
		buffer[3] = ((unsigned short)bRead >> (0 << 3)) & 0xff;
		printBuffer(buffer,packetSize,"Sent");
		int bSent = llwrite(buffer, bRead+4);
	}while(bRead == packetSize-4);
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
	ll.status = TRANSMITTER;
	setLL(ll);
    	
	memcpy(al.filename,"testfile",8);
	al.sequenceNumber = 0;

	if(ll.status == TRANSMITTER){
		llopen();
		sendStart();
		sendData();
		llclose();
	}else if(ll.status == RECEIVER) {
		llopen();
		receiveStart();
		receiveData();
	}
	
}