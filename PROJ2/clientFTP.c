#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <netdb.h>
#include <fcntl.h>
#include <strings.h>

typedef struct clientFTP {
	int sockfd, datafd;
	int serverPort, dataServerPort;
	char *serverAddr, *dataServerAddr;
	int maxSize;
}CLIENT_FTP;

CLIENT_FTP cf;

/* --PROBLEMS-- 
	- Download is not downloading the right size and is filling file with 0s
*/


int sendMsg(char * msg, int fd){
	printf("SENT: %s\n", msg);
	return write(fd, msg, strlen(msg));
}

int receiveMsg(char * msg, int fd){
	bzero(msg,cf.maxSize);
	read(fd, msg, cf.maxSize);
	printf("RECEIVED: %s\n", msg);

	return atoi(msg);
}

int tokenize(char *buf, char data[][cf.maxSize], char initChar, char endChar, char splitChar){
	int i = 0, j = 0, k = 0;
	for (i = 0; i < cf.maxSize ;i++){
		if(buf[i] == initChar){
			i++;
			break;
		}
	}

	for(j = 0; j < cf.maxSize && i < cf.maxSize; i++){
		if(buf[i] == splitChar){
			data[k][j] = '\0';
			j = 0;
			k++;
			continue;
		}
		if(buf[i] == endChar){
			break;
		}
		

		data[k][j] = buf[i];
		j++;
	}

	return 0;
}

int connectToData(char data[][cf.maxSize], struct sockaddr_in *server_addr){
	
	cf.dataServerAddr = malloc(cf.maxSize);
	bzero(cf.dataServerAddr,cf.maxSize);

	strcpy(cf.dataServerAddr,data[0]);
	strcat(cf.dataServerAddr,".");
	strcat(cf.dataServerAddr,data[1]);
	strcat(cf.dataServerAddr,".");
	strcat(cf.dataServerAddr,data[2]);
	strcat(cf.dataServerAddr,".");
	strcat(cf.dataServerAddr,data[3]);

	cf.dataServerPort = atoi(data[4]) * 0x100 + atoi(data[5]);

	printf("Data server addr: %s Data server port: %d\n",cf.dataServerAddr,cf.dataServerPort);

	/*server address handling*/
	bzero((char*) server_addr,sizeof(server_addr));
	server_addr->sin_family = AF_INET;
	server_addr->sin_addr.s_addr = inet_addr(cf.dataServerAddr);	/*32 bit Internet address network byte ordered*/
	server_addr->sin_port = htons(cf.dataServerPort);		/*server TCP port must be network byte ordered */
    
	/*open an TCP socket*/
	if ((cf.datafd = socket(AF_INET,SOCK_STREAM,0)) < 0) {
    	perror("socket()");
        exit(0);
    }

	/*connect to the server*/
    if(connect(cf.datafd, (struct sockaddr *) server_addr, sizeof(*server_addr)) < 0){
        perror("connect()");
		exit(0);
	}

	return cf.datafd;
}

int downloadFile(int fd){
	int bytes;
	int file = open("./download.zip", O_WRONLY | O_APPEND | O_CREAT, 0666);

	do{
		char buffer[cf.maxSize];
		bzero(buffer,cf.maxSize);

		if((bytes = read(fd, buffer, cf.maxSize)) < 0){
			printf("Couldn't read from server");
		}
		if (write(file,buffer,bytes) < 0){
			printf("Couldn't write to file\n");
		}
	}while(bytes > 0);

	return 0;
}

int main(int argc, char** argv){
	char wrBuf[256];
	char rdBuf[256];
	struct sockaddr_in server_addr, data_server_addr;
	int	bytes;
	struct hostent *h;

	cf.maxSize = 2048;
	cf.serverPort = 21;

    if(argc != 2) {
        fprintf(stderr, "Usage: %s hostname\n", argv[0]);
        return 0;
    }

    if((h=gethostbyname(argv[1])) == NULL) {
        herror("gethostbyname(): ");
        return 0;
    }
    else {
        cf.serverAddr = inet_ntoa(*((struct in_addr *)h->h_addr));
    	printf("Connecting to: %s\n",cf.serverAddr);
    }

	
	/*server address handling*/
	bzero((char*)&server_addr,sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(cf.serverAddr);	/*32 bit Internet address network byte ordered*/
	server_addr.sin_port = htons(cf.serverPort);		/*server TCP port must be network byte ordered */
    
	/*open an TCP socket*/
	if ((cf.sockfd = socket(AF_INET,SOCK_STREAM,0)) < 0) {
    	perror("socket()");
        exit(0);
    }

	/*connect to the server*/
    if(connect(cf.sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0){
        perror("connect()");
		exit(0);
	}

	if(receiveMsg(rdBuf,cf.sockfd) != 220){
		return 1;
	}

	if(sendMsg("USER anonymous\n",cf.sockfd) <= 0){
		return 1;
	}

	if(receiveMsg(rdBuf,cf.sockfd) != 331){
		return 1;
	}

	if(sendMsg("PASS  \n",cf.sockfd) <= 0){
		return 1;
	}

	if(receiveMsg(rdBuf,cf.sockfd) != 230){
		return 1;
	}

	if(sendMsg("PASV\n",cf.sockfd) <= 0){
		return 1;
	}

	if(receiveMsg(rdBuf,cf.sockfd) != 227){
		return 1;
	}	

	char responseBuffer[6][cf.maxSize];

	tokenize(rdBuf,responseBuffer,'(',')',',');
	connectToData(responseBuffer,&data_server_addr);

	/*
	
	if(sendMsg("LIST\n",cf.sockfd) <= 0){
		return 1;
	}

	if(receiveMsg(rdBuf,cf.sockfd) != 150){
		return 1;
	}
	
	receiveMsg(rdBuf,cf.datafd);

	*/

	if(sendMsg("RETR 100KB.zip\n",cf.sockfd) <= 0){
		return 1;
	}

	if(receiveMsg(rdBuf,cf.sockfd) != 150){
		return 1;
	}
	
	downloadFile(cf.datafd);

	if(receiveMsg(rdBuf,cf.sockfd) != 226){
		return 1;
	}

	if(sendMsg("QUIT\n",cf.sockfd) <= 0){
		return 1;
	}

	if(receiveMsg(rdBuf,cf.sockfd) != 221){
		return 1;
	}

	close(cf.sockfd);
	return 0;
}


