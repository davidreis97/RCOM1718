#ifndef LINK_LAYER_H
#define LINK_LAYER_H

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

typedef struct linkLayer {
    int fd;
    int status;
	char *port;
	int baudRate;
	unsigned int sequenceNumber;
	unsigned int timeout;
	unsigned int numTransmissions;
	struct termios oldtio;
}LINK_LAYER;

void setDebug(int d);

void setDelay(int delay);

int getDebug();

void setLL(LINK_LAYER linklayer);

LINK_LAYER getLL();

void setError(int percentage);

char getBCC(char *buffer, int size);

void printBuffer(char *buffer, int size, char *msg);

void changeBlocking(int block);

void parseAlarm();

int byteStuffing(char *buffer, int size);

int byteDestuffing(char *buffer, int size);

int testStuffing();

int send(char *buffer, unsigned int size);

int receive(char *buffer);

int timeoutAndSend(char *buffer, unsigned int size);

int llwrite(char *packet, unsigned int size);

int llclose();

int llread(char *buffer);

int llopen();

#endif
