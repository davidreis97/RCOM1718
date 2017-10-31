#include "linklayer.h"

const char FLAG = 0x7E;
const char ESCAPE = 0x7D;

const char SENDER_ADDRESS = 0x03;
const char RECEIVER_ADDRESS = 0x01;

const char CTRL_SET = 0b00000011;
const char CTRL_DISC = 0b00001011;
const char CTRL_CTRL[] = {0b00000000,0b01000000};
const char CTRL_UA = 0b00000111;
const char CTRL_RR[] = {0b00000101,0b010000101};
const char CTRL_REJ[] = {0b00000001,0b010000001};

const int BUFFER_SIZE = 65535;

int DEBUG;

char PREVIOUS_BCC2;
char PREVIOUS_FIRST_BYTE;

int ERROR_PERCENTAGE;
int DELAY;

LINK_LAYER ll;

int TIMEOUT_APPLIED = 0; int TIMEOUT_TRIES;

void setDebug(int d){
    DEBUG = d;
}

void setDelay(int delay){
    DELAY = delay;
}

int getDebug(){
    return DEBUG;
}

void setError(int percentage){
    ERROR_PERCENTAGE = percentage;
}

void setLL(LINK_LAYER linklayer){
    ll = linklayer;
}

LINK_LAYER getLL(){
    return ll;
}

void generateError(char *buffer, int size){
    buffer++; //Avoids changing the initial and last bit, which are errors usually corrected while stuffing
    size -= 2; 
    if(rand()%100 < ERROR_PERCENTAGE){
        printf("GENERATEERROR - Generating random mistake on frame\n");
        buffer[rand()%size] = 0xFF;
    }else{
        if (DEBUG) printf("GENERATEERROR - No error in this frame\n");
    }
    usleep(DELAY);
}

char getBCC(char *buffer, int size){
    char bcc = 0; //TODO incluir o proprio bcc na conta????
    int i;

    for (i = 0; i < size; i++){
        bcc ^= buffer[i];
    }

    return bcc;
}

void printBuffer(char *buffer, int size, char *msg){
    int i;    
    printf("%s: 0x", msg);
    for(i = 0; i < size; i++){
        printf("%02x ",(unsigned char) buffer[i]);    
    }
    printf("(%d bytes)\n", size);
}

void changeBlocking(int block){
    int flags = fcntl(ll.fd, F_GETFL, 0);
    if (block){
        fcntl(ll.fd, F_SETFL, flags & ~O_NONBLOCK);
    }else {
        fcntl(ll.fd, F_SETFL, flags | O_NONBLOCK);
    }
}

void parseAlarm() {
    printf("PARSEALARM - Timeout %d...\n", ll.numTransmissions - TIMEOUT_TRIES);
    TIMEOUT_APPLIED = 0;
    TIMEOUT_TRIES--;

    changeBlocking(0);
}

int byteStuffing(char *buffer, int size){
    char auxBuffer[BUFFER_SIZE];
    int bufC = 1, auxBufC = 1;
       
    auxBuffer[0] = FLAG;

    for(;bufC < size-1;bufC++){
        if(buffer[bufC] == FLAG || buffer[bufC] == ESCAPE){
            auxBuffer[auxBufC] = ESCAPE;
            auxBufC++;
            auxBuffer[auxBufC] = buffer[bufC] ^ 0x20;
        }else{
            auxBuffer[auxBufC] = buffer[bufC];  
        }

        auxBufC++;
    }
    
    auxBuffer[auxBufC] = FLAG;
    auxBufC++;
    
    memcpy(buffer,auxBuffer,auxBufC);
    return auxBufC;
}

int byteDestuffing(char *buffer, int size){
    char auxBuffer[BUFFER_SIZE];
    int bufC = 0, auxBufC = 0;

    for(;bufC < size;auxBufC++){
        if(buffer[bufC] == ESCAPE){
            bufC++;
            auxBuffer[auxBufC] = buffer[bufC] ^ 0x20;
        }else{
            auxBuffer[auxBufC] = buffer[bufC];        
        }
        
        bufC++;
    }

    memcpy(buffer,auxBuffer,auxBufC);
    return auxBufC;
}

int testStuffing(){
    char buffer[BUFFER_SIZE];
    int size;
    buffer[0] = FLAG;
    buffer[1] = SENDER_ADDRESS;
    buffer[2] = CTRL_CTRL[PAIR];
    buffer[3] = FLAG;
    buffer[4] = 0;
    buffer[5] = 0x12;
    buffer[6] = ESCAPE;
    buffer[7] = FLAG;
    
    printBuffer(buffer,8,"Initial buffer");
    size = byteStuffing(buffer,8);
    printBuffer(buffer,size,"After stuffing");
    size = byteDestuffing(buffer,size);
    printBuffer(buffer,size,"  Final buffer");
    return 0;
}

int send(char *buffer, unsigned int size){
    if (DEBUG) printBuffer(buffer,size,"SEND - To send, no stuffing");
    int stuffedSize = byteStuffing(buffer,size);
    int bytes = write(ll.fd,buffer,stuffedSize);
    if (DEBUG) printBuffer(buffer,bytes,"SEND -    Sent, w/ stuffing");
    return bytes;
}

int receive(char *buffer){
    int n = 0, ch=0;
    while(1){
        if((ch = read(ll.fd,buffer+n,1)) <=0 ){
            //Do nothing        
        }else if (n==0 && buffer[0] != FLAG){
            printf("READ - No flag on initial byte.\n");
            continue;
        }else if (n==1 && buffer[1] == FLAG){
            printf("READ - Flag after first byte.\n");
            continue;
        }else if (buffer[n] != FLAG && n == BUFFER_SIZE - 1){
            printf("READ - Exceeded buffer.\n");
            n = 0;
            continue;
        }else if (buffer[n] == FLAG && n > 2){
            break;
        }
        if (ch != 0){   
            n++;
        }   
    }

    generateError(buffer,n+1);
    
    if (DEBUG) printBuffer(buffer,n+1,"RECEIVED - no destuffing");
    int destuffedSize = byteDestuffing(buffer,n+1);
    if (DEBUG) printBuffer(buffer,destuffedSize,"RECEIVED - w/ destuffing");
    return destuffedSize;
}

int timeoutAndSend(char *sendBuffer, unsigned int size){
    if (DEBUG) printf("TIMEOUTANDSEND - Entering\n");
    int n = 0, ch=0, bytesWritten = 0, success = 0;
    char receiveBuffer[BUFFER_SIZE];

    TIMEOUT_TRIES = ll.numTransmissions;

    while(TIMEOUT_TRIES){

        signal(SIGALRM, parseAlarm);
        
        if(!TIMEOUT_APPLIED){  
            alarm(ll.timeout);
            TIMEOUT_APPLIED=1;

            bytesWritten = send(sendBuffer,size);

            if (DEBUG) printf("TIMEOUTANDSEND - Sent %d bytes and applied timeout to %ds\n",bytesWritten, ll.timeout);
        }

        changeBlocking(1);
    
        if((ch = read(ll.fd,receiveBuffer+n,1)) <=0 ){
            //Do nothing        
        }else if (n==0 && receiveBuffer[0] != FLAG){
            printf("TIMEOUTANDSEND - No flag on initial byte (%x)\n",receiveBuffer[0]);
            continue;
        }else if (n==1 && receiveBuffer[1] == FLAG){
            printf("TIMEOUTANDSEND - Flag right after first byte.\n");
            continue;
        }else if (receiveBuffer[n] != FLAG && n == BUFFER_SIZE - 1){
            printf("TIMEOUTANDSEND - Exceeded buffer.\n");
            n = 0;
            continue;
        }else if (receiveBuffer[n] == FLAG && n > 2){
            receiveBuffer[n+1] = '\0';
            success = 1;
            break;
        }
        if (ch > 0){
            n++;
        }
    }
    
    if (DEBUG) printBuffer(receiveBuffer,n+1,"RECEIVED - no destuffing");
    if (DEBUG) printf("TIMEOUTANDSEND - Canceling all alarms\n");
    alarm(0); //Cancels pending alarms
    TIMEOUT_APPLIED = 0;

    if (DEBUG) printBuffer(receiveBuffer,n+1,"TIMEOUTANDSEND - Received, no destuffing");
    int destuffedSize = byteDestuffing(receiveBuffer,n+1); 
    if (DEBUG) printBuffer(receiveBuffer,destuffedSize,"TIMEOUTANDSEND -Received w/ destuffing");   

    if(!success){
        bzero(receiveBuffer,255);
        printf("TIMEOUTANDSEND - Timed out after %d tries. \n", ll.numTransmissions);
        return -1;
    }else{
        generateError(receiveBuffer,n+1);
        memcpy(sendBuffer,receiveBuffer,BUFFER_SIZE);
    }

    if (DEBUG) printf("TIMEOUTANDSEND - Leaving. Received %d bytes and sent %d bytes.\n",destuffedSize, bytesWritten);
    return bytesWritten;
}

int llwrite(char *packet, unsigned int size){
    if (DEBUG) printf("LLWRITE - Entering\n");
    int bytesWritten;
    char buffer[BUFFER_SIZE];
    bzero(buffer, BUFFER_SIZE);

    (ll.sequenceNumber == PAIR) ? (ll.sequenceNumber = ODD) : (ll.sequenceNumber = PAIR); //Switches sequenceNumber
    
    resend:
    buffer[0] = FLAG;
    buffer[1] = SENDER_ADDRESS;
    buffer[2] = CTRL_CTRL[ll.sequenceNumber];
    buffer[3] = getBCC(buffer+1,2); //Block Check Character 1(BCC1)
    memcpy(buffer+4,packet,size);
    buffer[4+size] = getBCC(buffer+4,size); //Block Check Character 2(BCC2)
    buffer[4+size+1] = FLAG;

    if (DEBUG) printf("LLWRITE - Sending and Waiting\n");
    bytesWritten = timeoutAndSend(buffer,4+size+2);
    
    if(bytesWritten == -1){
        printf("LLWRITE - Timed out too many times.\n");
        
        return -1;     
    }else if(buffer[3] != getBCC(buffer+1,2)){
        printf("LLWRITE - Wrong BCC, Expected (%02X) but got (%02X)\n",(unsigned char)getBCC(buffer+1,2),(unsigned char)buffer[3]);
        printf("LLWRITE - Resending\n");

        goto resend;     
    }else if(buffer[2] == CTRL_REJ[ll.sequenceNumber]){
        printf("LLWRITE - Rejected: %02X , Current Parity: %d\n",(unsigned char) buffer[2], ll.sequenceNumber);
        printf("LLWRITE - Sending same packet again\n");    
        
        goto resend;
    }else if(buffer[2] == CTRL_RR[ll.sequenceNumber]){
        if (DEBUG) printf("LLWRITE - Successfully sent frame of (%d) bytes! Exiting function\n",bytesWritten);
        if (DEBUG) printf("LLWRITE - Leaving\n");
                
        return bytesWritten;
    }else{
        printf("LLWRITE - Failed to write on llwrite: Expected (%02X) but got unknown (%02X)\n",(unsigned char)CTRL_RR[ll.sequenceNumber], (unsigned char)buffer[2]);
        printf("LLWRITE - Resending\n");
        
        goto resend;
    }
}

int llclose(){
    if (DEBUG) printf("LLCLOSE - Entering\n");
    int bytesWritten = 0;
    char buffer[BUFFER_SIZE];
    bzero(buffer, BUFFER_SIZE);

    (ll.sequenceNumber == PAIR) ? (ll.sequenceNumber = ODD) : (ll.sequenceNumber = PAIR); //Switches sequenceNumber

    if(ll.status == TRANSMITTER){

        resend:

        buffer[0] = FLAG;
        buffer[1] = SENDER_ADDRESS;
        buffer[2] = CTRL_DISC;
        buffer[3] = getBCC(buffer+1,2); //Block Check Character (BCC)
        buffer[4] = FLAG;

        if (DEBUG) printf("LLCLOSE - Sending and waiting\n");
        bytesWritten = timeoutAndSend(buffer,5);
        
        if(bytesWritten == -1){
            printf("LLCLOSE - Timed out too many times.\n");
        
            return -1;     
        }else if(buffer[3] != getBCC(buffer+1,2)){
            printf("LLCLOSE - Wrong BCC, Expected (%02X) but got (%02X)\n",(unsigned char)getBCC(buffer+1,2),(unsigned char)buffer[3]);     
          
            goto resend;
        }else if(buffer[2] == CTRL_DISC){
            if (DEBUG) printf("LLCLOSE - Successfully received CTRL_DISC! Sending UA\n");
            
            buffer[0] = FLAG;
            buffer[1] = SENDER_ADDRESS;
            buffer[2] = CTRL_UA;
            buffer[3] = getBCC(buffer+1,2); //Block Check Character (BCC)
            buffer[4] = FLAG;

            send(buffer,5);
        }else if(buffer[2] == CTRL_REJ[PAIR] || buffer[2] == CTRL_REJ[ODD]){
            printf("LLCLOSE - Receiver returned REJ, resending DISC\n");
            
            goto resend;
        }else{
            printf("LLCLOSE - Failed to close on llclose: Expected (%02X) but got (%02X)\n",(unsigned char)CTRL_DISC, (unsigned char)buffer[2]);
            
            goto resend;
        }
    }else if(ll.status == RECEIVER){

        retry:

        buffer[0] = FLAG;
        buffer[1] = SENDER_ADDRESS;
        buffer[2] = CTRL_DISC;
        buffer[3] = getBCC(buffer+1,2); //Block Check Character (BCC)
        buffer[4] = FLAG;

        bytesWritten = timeoutAndSend(buffer,5);
        
        if(bytesWritten == -1){
            printf("LLCLOSE - Timed out too many times.\n");
        
            return -1;     
        }else if(buffer[3] != getBCC(buffer+1,2)){
            printf("LLCLOSE - Wrong BCC, Expected (%02X) but got (%02X)\n",(unsigned char)getBCC(buffer+1,2),(unsigned char)buffer[3]);  
            
            goto retry;  
        }else if(buffer[2] == CTRL_UA){
            if (DEBUG) printf("LLCLOSE - Received UA on llclose: Successfully closed!\n");
        }else if(buffer[2] == CTRL_DISC){
            printf("LLCLOSE - Transmitter resent DISC, resending DISC\n");
            goto retry;
        }else{
            printf("LLCLOSE - Failed to close on llclose: Expected (%02X) but got (%02X)\n",(unsigned char)CTRL_UA, (unsigned char)buffer[2]);
            goto retry;
        }
    }
    
    tcflush(ll.fd,TCIOFLUSH);
    tcsetattr(ll.fd,TCSANOW,&(ll.oldtio));
    close(ll.fd);
    if (DEBUG) printf("LLCLOSE - Leaving\n");
    return 0;   
}

int llread(char *buffer){
    int size;
    if (DEBUG) printf("LLREAD - Entering\n");

    (ll.sequenceNumber == PAIR) ? (ll.sequenceNumber = ODD) : (ll.sequenceNumber = PAIR); //Switches sequenceNumber
    
    retry:      
    
    size = receive(buffer);

    if (DEBUG) printBuffer(buffer,size,"LLREAD - Received");

    char retBuffer[BUFFER_SIZE];
    bzero(retBuffer, BUFFER_SIZE);

    char *dataBuffer = buffer + 4;
    int dataSize = size - 4 - 2;
    
    if(buffer[3] != getBCC(buffer+1,2)){
        printf("LLREAD - Wrong BCC-1, Expected (%02X) but got (%02X)\n",(unsigned char)getBCC(buffer+1,2),(unsigned char)buffer[3]);
        retBuffer[0] = FLAG;
        retBuffer[1] = SENDER_ADDRESS;
        retBuffer[2] = CTRL_REJ[ll.sequenceNumber];
        retBuffer[3] = getBCC(retBuffer+1,2); //Block Check Character (BCC)
        retBuffer[4] = FLAG;


        send(retBuffer,5);
        printf("LLREAD - CTRL_REJ sent, received %02X expected %02X\n", (unsigned char)buffer[2],(unsigned char) CTRL_CTRL[ll.sequenceNumber]);
        printf("LLREAD - Will retry to receive the same packet\n"); 
        
        goto retry;
    }else if(buffer[3] == PREVIOUS_BCC2 && dataBuffer[0] == PREVIOUS_FIRST_BYTE && buffer[2] == CTRL_CTRL[1 - ll.sequenceNumber]){
        printf("LLREAD - Caught previously received packet, resending RR");

        retBuffer[0] = FLAG;
        retBuffer[1] = SENDER_ADDRESS;
        retBuffer[2] = CTRL_RR[1 - ll.sequenceNumber];
        retBuffer[3] = getBCC(retBuffer+1,2); //Block Check Character (BCC)
        retBuffer[4] = FLAG;

        send(retBuffer,5);

        goto retry;
    }else if(buffer[2] == CTRL_CTRL[ll.sequenceNumber]){
        
        if(buffer[size-2] != getBCC(dataBuffer,dataSize)){
            printf("LLREAD - Wrong BCC-2, Expected (%02X) but got (%02X)\n",(unsigned char)getBCC(dataBuffer,dataSize),(unsigned char)buffer[size-2]);
            retBuffer[0] = FLAG;
            retBuffer[1] = SENDER_ADDRESS;
            retBuffer[2] = CTRL_REJ[ll.sequenceNumber];
            retBuffer[3] = getBCC(retBuffer+1,2); //Block Check Character (BCC)
            retBuffer[4] = FLAG;


            send(retBuffer,5);
            printf("LLREAD - CTRL_REJ sent, received %02X expected %02X\n", (unsigned char)buffer[2],(unsigned char) CTRL_CTRL[ll.sequenceNumber]);
            printf("LLREAD - Will retry to receive the same packet\n"); 

            goto retry;
        }   

        retBuffer[0] = FLAG;
        retBuffer[1] = SENDER_ADDRESS;
        retBuffer[2] = CTRL_RR[ll.sequenceNumber];
        retBuffer[3] = getBCC(retBuffer+1,2); //Block Check Character (BCC)
        retBuffer[4] = FLAG;

        send(retBuffer,5);

        PREVIOUS_BCC2 = buffer[3];
        PREVIOUS_FIRST_BYTE = dataBuffer[0];

        if (DEBUG) printf("LLREAD - CTRL_CTRL received, CTRL_RR sent\n");
        memmove(buffer,dataBuffer,dataSize);

        return dataSize;
    }else if(buffer[2] == CTRL_DISC){ 
        if (DEBUG) printf("LLREAD - CTRL_DISC received, calling llclose()\n");
        return llclose();
    }else if(buffer[2] == CTRL_SET){ //May happen when error occurs in receiver-sent UA packet - relies on one available timeout
        llopen();
        goto retry;
    }else{
        retBuffer[0] = FLAG;
        retBuffer[1] = SENDER_ADDRESS;
        retBuffer[2] = CTRL_REJ[ll.sequenceNumber];
        retBuffer[3] = getBCC(retBuffer+1,2); //Block Check Character (BCC)
        retBuffer[4] = FLAG;


        send(retBuffer,5);
        printf("LLREAD - CTRL_REJ sent, received %02X expected %02X\n", (unsigned char)buffer[2],(unsigned char) CTRL_CTRL[ll.sequenceNumber]);
        printf("LLREAD - Will retry to receive the same packet\n");        
        goto retry;
    }
}

int llopen(){
    if (DEBUG) printf("LLOPEN - Entering\n");
    
    struct termios newtio;
    
    ll.fd = open(ll.port, O_RDWR | O_NOCTTY);
    if (ll.fd <0) {perror(ll.port); exit(-1); }

    if ( tcgetattr(ll.fd,&(ll.oldtio)) == -1) {
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

    tcflush(ll.fd, TCIOFLUSH);

    if ( tcsetattr(ll.fd,TCSANOW,&newtio) == -1) {
        perror("tcsetattr");
        exit(-1);
    }

    int bytesWritten = 0;
    char buffer[BUFFER_SIZE];
    bzero(buffer, BUFFER_SIZE);
        
    if(ll.status == TRANSMITTER){
        resend:
        
        buffer[0] = FLAG;
        buffer[1] = SENDER_ADDRESS;
        buffer[2] = CTRL_SET;
        buffer[3] = getBCC(buffer+1,2); //Block Check Character (BCC)
        buffer[4] = FLAG;
        
        if (DEBUG) printf("LLOPEN - Sending CTRL_SET and waiting CTRL_UA\n");
        bytesWritten = timeoutAndSend(buffer,5);

        if(bytesWritten == -1){
            printf("LLOPEN - Timed out too many times.\n");
        
            return -1;     
        }else if(buffer[3] != getBCC(buffer+1,2)){
            printf("LLOPEN - Wrong BCC, Expected (%02X) but got (%02X)\n",(unsigned char)getBCC(buffer+1,2),(unsigned char)buffer[3]);  

            goto resend;   
        }else if(buffer[2] == CTRL_UA){
            if (DEBUG) printf("LLOPEN - Successfully received CTRL_UA\n");
        }else{
            printf("LLOPEN - Failed to open: Expected (%02X) but got (%02X)\n",(unsigned char)CTRL_UA, (unsigned char)buffer[2]);
            
            goto resend;
        }
    }else if(ll.status == RECEIVER){
        retry:
        
        receive(buffer);
        
        if(buffer[3] != getBCC(buffer+1,2)){
            printf("LLOPEN - Wrong BCC, Expected (%02X) but got (%02X)\n",(unsigned char)getBCC(buffer+1,2),(unsigned char)buffer[3]);
            
            goto retry;
        }else if(buffer[2] == CTRL_SET){
            buffer[0] = FLAG;
            buffer[1] = SENDER_ADDRESS;
            buffer[2] = CTRL_UA;
            buffer[3] = getBCC(buffer+1,2); //Block Check Character (BCC)
            buffer[4] = FLAG;

            send(buffer,5);
            if (DEBUG) printf("LLOPEN - Sent CTRL_UA\n");
        }else{
            printf("LLOPEN - Failed to open: Expected (%02X) but got (%02X)\n",(unsigned char)CTRL_UA, (unsigned char)buffer[2]);
            
            goto retry;
        }
    }

    if (DEBUG) printf("LLOPEN - Leaving\n");

    return 0;
}
