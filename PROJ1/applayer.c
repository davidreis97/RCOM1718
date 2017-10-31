#include "linklayer.h"

const char C_DATA = 1;
const char C_START = 2;
const char C_END = 3; 

const char T_SIZE = 0;
const char T_NAME = 1;

const char S_SIZE = 4; 
const unsigned char S_NAME = 255;

const int APP_BUFFER_SIZE = 65525; //2^16 - 10 bits for link layer
int APP_PACKET_SIZE = 255;


typedef struct appLayer {
    int fd;
    char *filename;
    int expectedSize;
    unsigned char sequenceNumber;
    int progress;
}APP_LAYER;

APP_LAYER al;

void printProgress(int percentage, char message[], int divisions){
	int i = 0;
	if(percentage > 100) percentage = 100;
	printf("\r[%s]: [",message);
	for(i = 0; i <= 100; i += divisions){
		if(percentage < i){
			printf(" ");
		}else{
			printf("=");
		}
	}
	printf("] %d%%",percentage);
	fflush(stdout);
}

int setFileSize(char *buffer){
	al.fd = open(al.filename,O_RDONLY);

	if(al.fd <= 0) {
		printf("SETFILESIZE : Could not open file [%s]\n",al.filename);
		return -1;
	}

    struct stat buf;
	fstat(al.fd, &buf);
	al.expectedSize = buf.st_size;

	if (getDebug()) printf("Found file with size (%x)\n",al.expectedSize);

	buffer[0] = ((unsigned int)al.expectedSize >> (3 << 3)) & 0xff; //Assures Big Endianness - works on both systems
	buffer[1] = ((unsigned int)al.expectedSize >> (2 << 3)) & 0xff;
	buffer[2] = ((unsigned int)al.expectedSize >> (1 << 3)) & 0xff;
	buffer[3] = ((unsigned int)al.expectedSize >> (0 << 3)) & 0xff;

	return 0;
}

int setFileName(char *buffer){
	memcpy(buffer, al.filename, strlen(al.filename));
	bzero(buffer + strlen(al.filename),S_NAME - strlen(al.filename));

	return 0;
}

int getFileSize(unsigned char *buffer){
	al.expectedSize = 0x1000000*buffer[0] +
					  0x10000*buffer[1] +
					  0x100*buffer[2] +
					  buffer[3];

	return 0;
}

char getFileBCC(){
	//TODO
}

int getFileName(char *buffer){
	memcpy(al.filename,buffer,S_NAME);

	return 0;
}

int sendStart(){
	char buffer[APP_BUFFER_SIZE];

	buffer[0] = C_START;
	buffer[1] = T_SIZE;
	buffer[2] = S_SIZE;

	if (setFileSize(buffer + 3) != 0){
		printf("SENDSTART: Could not set file size\n");
		return -1;
	}

	buffer[3 + S_SIZE] = T_NAME;
	buffer[4 + S_SIZE] = S_NAME;

	if (setFileName(buffer + 5 + S_SIZE) != 0){
		printf("SENDSTART: Could not set file name\n");
		return -1;
	}

	return llwrite(buffer,5 + S_SIZE + S_NAME);
}

int receiveStart(){ //FIX - File name not making it completely sometimes
	char buffer[APP_BUFFER_SIZE];
	bzero(buffer,APP_BUFFER_SIZE);
	int c = llread(buffer);
	int i = 0;
	
	if(buffer[0] != C_START){
		printf("RECEIVESTART: Received Wrong Packet - Not Start\n");
		return -1;
	}else if(c < 0){
		printf("RECEIVESTART: llread returned error\n");
		return -1;
	}

	for(i = 1; i < c; ){
		if(buffer[i] == T_SIZE){
			if(getFileSize(buffer + i + 2) != 0){
				printf("RECEIVESTART: Could not get file size\n");
			}
		}else if(buffer[i] == T_NAME){
			if(getFileName(buffer + i + 2) != 0){
				printf("RECEIVESTART: Could not get file name\n");
			}
		}else{
			printf("RECEIVESTART: Unknown type: %x Skipping it\n",buffer[i]);
		}

		i++;
		i += (unsigned char) buffer[i];
		i++;
	}

	printf("RECEIVESTART: Found file called %s with a size of %d bytes\n",al.filename,al.expectedSize);
}

int sendEnd(){ //Add more stuff? If not, move to add data and receive data
	char buffer[APP_PACKET_SIZE];

	buffer[0] = C_END;
	//buffer[1] = getFileBCC();

	if(llwrite(buffer, APP_PACKET_SIZE) <= 0){
		printf("SENDEND: Could not send end packet\n");
		return -1;
	}else{
		return 0;
	}
}

int receiveEnd(char *buffer){
	if(buffer[0] != C_END){
		printf("RECEIVEEND: Expected end packet (%x) but got (%x)\n",C_END,buffer[0]);
		return -1;
	}/*else if(buffer[1] != getFileBCC()){
		printf("RECEIVEEND: Got wrong file BCC, expected (%x) got (%x)",getFileBCC(), buffer[1]);
	}*/
}

int receiveData(){
	char buffer[APP_BUFFER_SIZE];
	bzero(buffer,APP_BUFFER_SIZE);
	int bRead = 0, writeFd, totalread = 0;

	if ((writeFd = open(al.filename,O_WRONLY | O_APPEND | O_CREAT | O_EXCL, 0666)) <= 0){ //Currently creating file with wrong permissions.
		printf("RECEIVEDATA: Could not open file [%s] for write. (File already exists?)\n",al.filename);
		return -1;
	}

	printf("Expected filesize: %d\n", al.expectedSize);

	do{
		bzero(buffer,APP_BUFFER_SIZE);
		bRead = llread(buffer);
		totalread += bRead - 4;


		if(getDebug()) printBuffer(buffer,bRead,"RECEIVEDATA: Received packet");

		if(bRead == 0){
			if(getDebug()) printf("RECEIVEDATA: Got bRead == 0, assuming llclose succeeded\n");
			break;
		}

		if(buffer[0] == C_END){
			if (receiveEnd(buffer) != 0){
				printf("RECEIVEDATA: Error processing end packet\n");
				return -1;
			}
			continue;
		}else if(bRead < 5 && bRead != 0){
			printf("RECEIVEDATA: Packet received was too short (%d bytes)\n",bRead);
			return -1;
		}else if(buffer[0] != C_DATA){
			printBuffer(buffer,bRead,"RECEIVEDATA: Received non data/end packet\n");
			return -1;
		}else if((unsigned char) buffer[1] != al.sequenceNumber++){
			printf("RECEIVEDATA: Packets desynchronized expected (%d) got (%d)\n",al.sequenceNumber,buffer[1]);
			return -1;
		}else if(bRead - 4 != (0x100*(unsigned char) buffer[2]) + (unsigned char) buffer[3]){
			printf("RECEIVEDATA: Packet has wrong size expected (%d) got (%d) \n",
				   bRead - 4, (0x100*(unsigned char) buffer[2]) + (unsigned char) buffer[3]);
			return -1;
		}

		if (!getDebug() && al.progress) printProgress((totalread*100/al.expectedSize), al.filename, 5);

		
		if(bRead != 0) {
			if (write(writeFd, buffer + 4, bRead - 4) != bRead - 4){
				printf("RECEIVEDATA: Could not write to file.\n");
				return -1;
			}
		}

	}while(bRead > 0);

	struct stat buf;
	fstat(writeFd, &buf);
	
	if(al.expectedSize != buf.st_size){
		printf("RECEIVEDATA: Final file size (%ld) does not match size provided by start packet (%d)\n",buf.st_size,al.expectedSize);
		return -1;
	}else{
		return al.expectedSize;
	}
}

int sendData(){
	int bRead = 0, totalread = 0;
	char buffer[APP_PACKET_SIZE];

	do{
		buffer[0] = C_DATA;
		buffer[1] = al.sequenceNumber++;
		bzero(buffer+4,APP_PACKET_SIZE-4);
		bRead = read(al.fd, buffer+4, APP_PACKET_SIZE-4);
		totalread += bRead;

		if (!getDebug() && al.progress) printProgress((totalread*100/al.expectedSize), al.filename, 5);

		if(bRead < 0){
			printf("SENDDATA: Could not read from file\n");
			return -1;
		}else if(bRead > 0){
			buffer[2] = ((unsigned short)bRead >> (1 << 3)) & 0xff;
			buffer[3] = ((unsigned short)bRead >> (0 << 3)) & 0xff;

			if(getDebug()) printBuffer(buffer,bRead+4,"SENDDATA: Sending packet");
		
			if (llwrite(buffer, bRead+4) <= 0){
				printf("SENDDATA: Could not send packet\n");
				return -1;
			}
		}
	}while(bRead > 0);
}

int processArgs(int argc, char*argv[]){	
	int i = 0;
	LINK_LAYER ll;

	setDebug(0);

	ll.port = "/dev/ttyS0";
	ll.baudRate = B38400;
	ll.sequenceNumber = PAIR;
	ll.timeout = 3;
	ll.numTransmissions = 3;
	al.progress = 0;
	ll.status = RECEIVER;
	
	for(i = 1; i < argc; i++){
		if(strcmp(argv[i],"--debug") == 0){
			setDebug(1);
		}else if(strcmp(argv[i],"-T") == 0 && i+1 < argc){
			i++;
			ll.status = TRANSMITTER;
			al.filename = argv[i];
		}else if(strcmp(argv[i],"-p") == 0 && i+1 < argc){
			i++;
			ll.port = argv[i];
		}else if(strcmp(argv[i],"-t") == 0 && i+1 < argc){
			i++;
			ll.timeout = atoi(argv[i]);
		}else if(strcmp(argv[i],"-ps") == 0 && i+1 < argc){
			i++;
			APP_PACKET_SIZE = atoi(argv[i]);
		}else if(strcmp(argv[i],"--progress") == 0){
			al.progress = 1;
		}else{
			printf("Usage: %s [--debug] [-T : Set file to send (RECEIVER by default)] [-p : Set port (%s by default)] [-t : Set timeout (%d by default)] [-ps : Set packet size (%d by default)] [--progress : Show transfer progress (%d by default)]\n" ,argv[0], ll.port, ll.timeout, APP_PACKET_SIZE, al.progress);
			return -1;
		}
	}	

	setLL(ll);

	return 0;
}

int main(int argc, char*argv[]){
    
	srand(time(NULL));

	if (processArgs(argc, argv) != 0){
		return -1;
	}

  	al.sequenceNumber = 0;

	if(getLL().status == TRANSMITTER){
		if (llopen() < 0) {
			return -1;
		}
		if (sendStart() < 0){
			return -1;
		}
		if (sendData() < 0){
			return -1;
		}
		if (sendEnd() < 0){
			return -1;
		}
		if(llclose()){
			return -1;
		}
	}else if(getLL().status == RECEIVER) {
		al.filename = malloc(S_NAME);
		if (llopen() < 0){
			return -1;
		}
		if (receiveStart() < 0){
			return -1;
		}
		if (receiveData() < 0){
			return -1;
		}
		free(al.filename);
	}
	printf("\nSuccess!\n");
	close(al.fd);
}
