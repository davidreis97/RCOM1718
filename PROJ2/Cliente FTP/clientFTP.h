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

#define MAX_SIZE 2048

#define FTP_READY 220
#define FTP_NEED_PASSWORD 331
#define FTP_LOGGED_IN 230
#define FTP_ENTERED_PASSIVE_MODE 227
#define FTP_FILE_STATUS_OK 150
#define FTP_CLOSING_DATA_CONNECTION 226
#define FTP_CLOSING_CONTROL_CONNECTION 221

typedef struct clientFTP {
	int sockfd, datafd;
	int serverPort, dataServerPort;
	char *serverAddr, *dataServerAddr;
	int maxSize;
	int authenticating;
}CLIENT_FTP;

typedef struct ftpUrl {
	char username[256];
	char password[256];
	char host[256];
	char filepath[256];
}FTP_URL;

FTP_URL fu;
CLIENT_FTP cf;

int sendMsg(char * msg, int fd);

int receiveMsg(char * msg, int fd);

int tokenize(char *buf, char data[][cf.maxSize], char initChar, char endChar, char splitChar);

int connectToData(char data[][cf.maxSize], struct sockaddr_in *server_addr);

int downloadFile(int fd, char *filename);

void parser(int argc, char *argv[]);
