#include "clientFTP.h"

int sendMsg(char * msg, int fd){
	printf("SENT: %s\n", msg);
	return write(fd, msg, strlen(msg));
}

int receiveMsg(char * msg, int fd){
	bzero(msg,MAX_SIZE);
	read(fd, msg, MAX_SIZE);
	printf("RECEIVED: %s\n", msg);

	return atoi(msg);
}

int tokenize(char *buf, char data[][MAX_SIZE], char initChar, char endChar, char splitChar){
	int i = 0, j = 0, k = 0;
	for (i = 0; i < MAX_SIZE ;i++){
		if(buf[i] == initChar){
			i++;
			break;
		}
	}

	for(j = 0; j < MAX_SIZE && i < MAX_SIZE; i++){
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

int connectToData(char data[][MAX_SIZE], struct sockaddr_in *server_addr){
	
	cf.dataServerAddr = malloc(MAX_SIZE);
	bzero(cf.dataServerAddr,MAX_SIZE);

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

int downloadFile(int fd, char *filename){
	int bytes;
	int file = open(filename, O_WRONLY | O_APPEND | O_CREAT | O_EXCL, 0666);

	do{
		char buffer[MAX_SIZE];
		bzero(buffer,MAX_SIZE);

		if((bytes = read(fd, buffer, MAX_SIZE)) < 0){
			printf("Couldn't read from server");
		}
		if (write(file,buffer,bytes) < 0){
			printf("Couldn't write to file\n");
		}
	}while(bytes > 0);

	return 0;
}

void parser(int argc, char *argv[]) {
	if (sscanf(argv[1],"ftp://%[^:]%*[:]%[^@]%*[@]%[^/]%s", fu.username, fu.password, fu.host, fu.filepath) == 4) {
		cf.authenticating = 1;
		printf("\nUsername: [%s]\nHost: [%s]\nFilepath: [%s]\n\n", fu.username, /*fu.password,*/ fu.host, fu.filepath);
	}else if (sscanf(argv[1],"ftp://%[^/]%s", fu.host, fu.filepath) == 2){
		cf.authenticating = 0;
		printf("Warning: Couldn't parse any authentication\n\n"); //TODO - Fallback not working (solution: try to find @ or : in the string)
		printf("Host: [%s]\nFilepath: [%s]\n\n", fu.host, fu.filepath);
	}else{
		printf("Error: Couldn't parse ftp url\n");
		exit(1);
	}
}

int main(int argc, char* argv[]){
	char wrBuf[MAX_SIZE];
	char rdBuf[MAX_SIZE];
	char msg[MAX_SIZE];
	struct sockaddr_in server_addr, data_server_addr;
	int	bytes;
	struct hostent *h;

	if (argc != 2) {
		printf("Usage: %s ftp://[<user>:<password>@]<host>/<url-path>\n",argv[0]);
		exit(1);
	}

	parser(argc, argv);

	cf.serverPort = 21;

    if((h=gethostbyname(fu.host)) == NULL) {
        herror("gethostbyname(): ");
        return 0;
    }
    else {
        cf.serverAddr = inet_ntoa(*((struct in_addr *)h->h_addr));
    	printf("Connecting to: %s:%d\n",cf.serverAddr, cf.serverPort);
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

	if(receiveMsg(rdBuf,cf.sockfd) != FTP_READY){
		return 1;
	}


	if(cf.authenticating){
		strcpy(msg,"USER ");
		strcat(msg,fu.username);
		strcat(msg,"\n");

		if(sendMsg(msg,cf.sockfd) <= 0){
			return 1;
		}

		if(receiveMsg(rdBuf,cf.sockfd) != FTP_NEED_PASSWORD){ //ALWAYS ASSUMES PASSWORD IS NEEDED
			return 1;
		}

		strcpy(msg,"PASS ");
		strcat(msg,fu.password);
		strcat(msg,"\n");

		if(sendMsg(msg,cf.sockfd) <= 0){
			return 1;
		}

		if(receiveMsg(rdBuf,cf.sockfd) != FTP_LOGGED_IN){
			return 1;
		}
	}

	if(sendMsg("PASV\n",cf.sockfd) <= 0){
		return 1;
	}

	if(receiveMsg(rdBuf,cf.sockfd) != FTP_ENTERED_PASSIVE_MODE){
		return 1;
	}	

	char responseBuffer[6][MAX_SIZE];

	tokenize(rdBuf,responseBuffer,'(',')',',');
	connectToData(responseBuffer,&data_server_addr);

	/* //LIST DIRECTORY
	
	if(sendMsg("LIST\n",cf.sockfd) <= 0){
		return 1;
	}

	if(receiveMsg(rdBuf,cf.sockfd) != FTP_FILE_STATUS_OK){
		return 1;
	}
	
	receiveMsg(rdBuf,cf.datafd);

	*/

	
	strcpy(msg,"RETR ");
	strcat(msg,fu.filepath);
	strcat(msg,"\n");

	if(sendMsg(msg,cf.sockfd) <= 0){
		return 1;
	}

	if(receiveMsg(rdBuf,cf.sockfd) != FTP_FILE_STATUS_OK){
		return 1;
	}
	
	downloadFile(cf.datafd,fu.filepath);

	if(receiveMsg(rdBuf,cf.sockfd) != FTP_CLOSING_DATA_CONNECTION){
		return 1;
	}

	if(sendMsg("QUIT\n",cf.sockfd) <= 0){
		return 1;
	}

	if(receiveMsg(rdBuf,cf.sockfd) != FTP_CLOSING_CONTROL_CONNECTION){
		return 1;
	}

	close(cf.sockfd);
	return 0;
}
