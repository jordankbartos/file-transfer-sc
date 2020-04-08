/*******************************************************************************
  fork_connection_q(client_port)
 * Name:          Jordan K Bartos
 * Last Modified: Feb 6, 2020
 * File:          chatclient.c
 * Course:        CS-372-400 W2020
 * Description:   this program runs from the command line. It connects to the
 *   chatserver program and forms a TCP connection. Once a connection is formed,
 *   the two end systems become peers and can exchange messages back and forth.
 *   Messages entered on one end are displayed on the other end and vice versa.
 *   The parts of this program that handle the TCP connection are adapted from
 *   my CS344 OTP assignment, which in turn was adapted from Ben Brewster's 
 *   CS344 lecture materials.
 * Input:
 *   argv[1] - name of host that chatserver is running on
 *   argv[2] - port number that chatserver is listening on
 *   
 * note: this program attempts to earn extra credit by allowing asynchronous
 *   sending/receiving of data over the TCP connection by use of SIGIO signals.
 *   This allows each user to send multiple messages at a time and to receive
 *   a message at any time.
*******************************************************************************/
#include <sys/types.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <dirent.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <netdb.h> 

#define TRUE 1
#define FALSE 0
#define MAX_PORT_NUMBER 65535
#define MIN_PORT_NUMBER 1
#define BUFFER_LENGTH 1024
#define MAX_COMMAND_LENGTH 10
#define MAX_FILE_NAME_LENGTH 255
#define LIST_CODE 5322345
#define TRANSFER_CODE 9912349
#define CD_CODE 126649

char LIST_ALL_COMMAND[4] = "-la";
char LIST_COMMAND[3] = "-l";
char TRANSFER_COMMAND[3] = "-g";
char MESSAGE_DIVIDER = '#';
char CD_COMMAND[3] = "-c";
int SHOW_HIDDEN = FALSE;

//global variables for signal-handler exit
char* sendString;
char* buffer;


/*******************************************************************************
 *                void validateArgs(int argc, char* argv[])
 * Description: ensures that the supplied command-line arguments to this program
 *   are valid. There should only be one argument, a port number on which to run
 *   the server. It must be an integer between 1 and 65535
 * Input:
 *   int argc - the number of arguments supplied to the process
 *   char* argv[] - an array of pointers to char containing the passed-in 
 *                  arguments
 * Output:
 *   none
 * Postconditions: the program is terminated if the arguments are invalid.
 *   Otherwise, execution continues normally
*******************************************************************************/
void validateArgs(int argc, char* argv[]) {
  if (argc != 2) {
    fprintf(stderr, "ERROR: %d arguments supplied. Expected 1\n", argc -1);
    exit(1);
  }

  int portNumber = atoi(argv[1]);
  if (portNumber < MIN_PORT_NUMBER || portNumber > MAX_PORT_NUMBER) {
    fprintf(stderr, "ERROR: %s is not a valid port number.\n", argv[1]);
    exit(1);
  }
}


/*******************************************************************************
 *                   void catchSIGINT(int sigNumber)
 * Description: This function is the SIGINT handler. It frees dynamically
 *   allocated memory and terminates the process
 * Input: sigNumber - the signal number that caused the interrupt
 * Output: none
 * Preconditions: the process receives a SIGINT from the user
 * Postconditions: the process exits gracefully
*******************************************************************************/
void catchSIGINT(int sigNumber){
  // free all dynamically allocated memory
  free(sendString);
  free(buffer);
  exit(0);
}

/*******************************************************************************
 *                    void catchSIGPIPE(int sigNumber)
 * Description: This function is the signal handler for the SIGPIPE signal that
 *   arrives when ftclient shuts down before closing one/both of the connections
 * Input: sigNumber - the signal number that caused the interrupt
 * Output: None
 * Preconditions: the process receives a SIGIO due to a prematurely closed
 *   connection
 * Postconditions: the process exits gracefully
*******************************************************************************/
void catchSIGPIPE(int sigNumber){
  // free all dynamically allocated memory
  fprintf(stderr, "sigpipe encountered\n");
  free(buffer);
  free(sendString);
  exit(3);
}


/*******************************************************************************
*                     void setSignalHandler()
* This code is adapted from my CS344 smallsh program (assignment 3). And that
* code was adapted from CS344 lectures in block 3 by Benjamin Brewster
* 
* Description: this function sets up the signal handler for SIGINT and SIGIO
* Input: None
* Output: None
* Preconditions: None
* Postconditions: The process has signal handlers in place that will catch 
* SIGINT and SIGPIPE signals and handle them accordingly
*******************************************************************************/
void setSignalHandler() {
  // SIGINT
  struct sigaction SIGINT_action = {{0}};
  SIGINT_action.sa_handler = catchSIGINT;
  sigfillset(&SIGINT_action.sa_mask);
  SIGINT_action.sa_flags = 0;
  sigaction(SIGINT, &SIGINT_action, NULL);

  //SIGPIPE
  struct sigaction SIGPIPE_action = {{0}};
  SIGPIPE_action.sa_handler = catchSIGPIPE;
  sigfillset(&SIGPIPE_action.sa_mask);
  SIGPIPE_action.sa_flags = 0;
  sigaction(SIGPIPE, &SIGPIPE_action, NULL);
}


/*******************************************************************************
 *          void activateListenSocket(int*, int*, struct sockaddr_in*)
 * Sources cited: The code for this function is adapted from my CS344 OTP
 *   project, and the code for that was adapted from lecture materials for CS344
 *   provided by Benjamin Brewster
 *
 * Description: This function creates, binds, and activates a socket for 
 *   connection P.
 * Input:
 *   int* listenSocketFD - pointer to int that will hold the file descriptor for
 *     connection P
 *   int* portNumber - pointer to int that holds the port number to listen on
 *   struct sockaddr_in* - pointer to a struct sockaddr_in that will be
 *     populated with the data needed to open/run the socket
 * Output:
 *   none
 * Preconditions: none
 * Postconditions: ftserver is listening on a socket, connection P, for incoming
 *   connections from ftclient
*******************************************************************************/
void activateListenSocket(int* listenSocketFD, 
                     int* portNumber, 
                     struct sockaddr_in* serverAddress) {
  /* All socket programming code is adapted from my CS344 OTP assignment which 
     was, in turn, adapted from Ben Brewster's CS344 lectures */

  /* set up the address struct for the server socket */
  memset((char*)serverAddress, '\0', sizeof(*serverAddress));
  serverAddress->sin_family = AF_INET;
  serverAddress->sin_port = htons(*portNumber);
  serverAddress->sin_addr.s_addr = INADDR_ANY;

  /* set up the socket */
  *listenSocketFD = socket(AF_INET, SOCK_STREAM, 0);
  if (listenSocketFD < 0) {
    fprintf(stderr, "ERROR opening socket\n");
    exit(1);
  }
      
  /* attempt to bind the socket for listening for connections */
  if (bind(*listenSocketFD, (struct sockaddr*)serverAddress, sizeof(*serverAddress)) < 0) {
    fprintf(stderr, "ERROR on binding socket\n");
    exit(1);
  }
  listen(*listenSocketFD, 5);
}


/*******************************************************************************
 *                 void openConnectionQ(char*, int)
 * Sources cited: The code for this function is adapted from my CS344 OTP
 *   project, and the code for that was adapted from lecture materials for CS344
 *   provided by Benjamin Brewster
 *
 * Description: This function creates, binds, and activates a socket for 
 *   connection Q. It then connects the socket to the listening ftclient process
 *   and returns the socket connection file descriptor
 * Input:
 *   char* clientName - the host name or IP address of the ftclient
 *   int portNumber - the port number that client is listening on
*******************************************************************************/
int openConnectionQ(char* clientName, int clientPort) {

  struct sockaddr_in clientAddress;
  struct hostent* clientInfo;

  // populate the client address sockaddr_in struct with the correct data
  memset((char*)&clientAddress, '\0', sizeof(clientAddress));
  clientAddress.sin_family = AF_INET;
  clientAddress.sin_port = htons(clientPort);
  clientInfo = gethostbyname(clientName);
  if (clientInfo == NULL) {
    fprintf(stderr, "ERROR: Could not connect to client on connection Q\n");
    exit(2);
  }
  memcpy((char*)&clientAddress.sin_addr.s_addr, (char*)clientInfo->h_addr, clientInfo->h_length);

  // create a socket
  int socketFD = socket(AF_INET, SOCK_STREAM, 0);
  if (socketFD < 0) {
    fprintf(stderr, "ERROR: Could not open socket connection Q\n");
    exit(2);
  }

  // use the socket and clientAddress struct to open a TCP connection to ftclient
  // on connection Q
  if (connect(socketFD, (struct sockaddr*)&clientAddress, sizeof(clientAddress)) < 0) {
    fprintf(stderr, "ERROR: Could not connect on socket connetion Q\n");
    fprintf(stderr, "%d: %s\n", errno, strerror(errno));
    exit(2);
  }
  return socketFD;
}
 




/*******************************************************************************
 *     void parseCommands(char* buffer, char* command, char* fileName)
 * Description: This function parses the commands that were sent from the client
 *   process. It sets command and fileName to point to their respective
 *   information
 * Input:
 *   - char* buffer - this is a pointer to a char array that contains the string
 *                    of characters received from the client process
 *   - char** command - this is a pointer to an uninitialized char*
 *   - char** fileName - a pointer to an uninitialized char*
 * Output:
 *   - none
 * Preconditions:
 *   - buffer points to an array of characters received from client
 * Postconditions:
 *   - the contents of buffer are modified to create 2 sub-strings as follows:
 *   - *command points to the command that was sent
 *   - *fileName points to the fileName that was sent
 *   - Instances of MESSAGE_DIVIDER that are encountered are replaced with '\0'
*******************************************************************************/
void parseCommands(char* buffer, char** command, char** fileName, int* qClientPort) {
  char* curr = buffer;
  *command = buffer;
  *fileName = NULL;
  char* qClientPortStr = NULL;
  
  while(*curr != '\0') {
    if (*curr == MESSAGE_DIVIDER) {
      if (*fileName == NULL) {
        *curr = '\0';
        *fileName = curr + 1;
      }
      else if (qClientPortStr == NULL){
        *curr = '\0';
        qClientPortStr = curr + 1;
      }
      else {
        *curr = '\0';
      }
    }
    curr += 1;
  }
  *qClientPort = atoi(qClientPortStr);
}


/*******************************************************************************
 *                         int getCommandCode(char* command)
 * Description: analyzes the command received from ftclient and determines
 *   whether the command is -l (list) or -g (get) or -c (cd)
 * Input: char* command - a c-string containing the entire message recieved from
 *   ftclient
 * Output: the integer code for the given command
 * Preconditions: a message was received that contains a valid command from 
 *   ftclient
 * Postconditions: none
*******************************************************************************/
int getCommandCode(char* command) {
  if(strcmp(command, TRANSFER_COMMAND) == 0) {
    return TRANSFER_CODE;
  }
  if(strcmp(command, LIST_COMMAND) == 0) {
    SHOW_HIDDEN = FALSE;
    return LIST_CODE;
  }
  if(strcmp(command, LIST_ALL_COMMAND) == 0) {
    SHOW_HIDDEN = TRUE; 
    return LIST_CODE;
  }
  if(strcmp(command, CD_COMMAND) == 0) {
    return CD_CODE;
  }
  return -1;
}


/*******************************************************************************
 *                void sendDirectoryContents(int, int, char*, int)
 * Sources cited: https://www.geeksforgeeks.org/c-program-list-files-sub-directories-directory/
 * 
 * Description: This function sends a listing of the files in the current
 *   directory to ftclient on connection Q
 * Input:
 *   int connectionP_FD - the file descriptor for the connection P socket
 *   int connectionQ_FD - the file descriptor for the connection Q socket
 *   char* clientIP - a c-string containing the host name or IP address of
 *     the host machine that ftclient is running on
 *   int clientPort - the port number that ftclient is listening on for
 *     connection Q
 * Output:
 *   none
*******************************************************************************/
void sendDirectoryContents(int connectionP_FD, int connectionQ_FD, char* clientIP, int clientPort, int hostPort) {
  printf("List directory requested on port %d\n", hostPort); fflush(stdout);

  // open directory information
  struct dirent* directoryEntry;
  DIR *dir = opendir(".");
  // get a safe amount of memory for filenames
  sendString = malloc(sizeof(char) * (MAX_FILE_NAME_LENGTH + 512));
  memset(sendString, '\0', MAX_FILE_NAME_LENGTH + 512);
  if(dir == NULL) {
    strcpy(sendString, "Error opening directory");
    send(connectionP_FD, sendString, strlen(sendString), 0);
  }
  else {
    printf("Sending directory contents to %s:%d\n", clientIP, clientPort); fflush(stdout);
    // read the name of each file or directory in turn
    while((directoryEntry = readdir(dir)) != NULL) {
      memset(sendString, '\0', MAX_FILE_NAME_LENGTH + 512);
      strcpy(sendString, directoryEntry->d_name);
      if(directoryEntry->d_type == DT_DIR) {
        strcat(sendString, "/");
      }
      // don't send hidden files or . and ..
      if(SHOW_HIDDEN || sendString[0] != '.') {
        sendString[strlen(sendString)] = '\n';
        // send the name of the directory/file
        send(connectionQ_FD, sendString, strlen(sendString), 0);
      }
    }
    // close the directory
    closedir(dir);
    // close the connection
    close(connectionQ_FD);
  }
  free(sendString);
  sendString = NULL;
}


int min(int a, int b) {
  if (a < b) {
    return a;
  }
  return b;
}


/*******************************************************************************
 *             void sendFile(int, int, char*, int, int)
 * sources cited: https://www.programmingsimplified.com/c-program-read-file
 *                https://stackoverflow.com/questions/25634483/send-binary-file-over-tcp-ip-connection
 * 
 * Description: this function reads a file and sends it over connection Q to
 *   ftclient
 * Input: 
 *   int connectionP_FD - the file descriptor for connection P
 *   int connectionQ_FD - the file descriptor for connection Q
 *   char* filename - the name of the file to be sent
 *   char* clientiP - the IP address of the client
 *   int clientPort - so it can be displayed
 *   int hostPort - so it can be displayed
 * Output:
 *   none
 * Preconditions:
 *   - connectionP and connectionQ have been established
 *   - a file by the name of filename exists in the current directory
 * Postconditions:
 *   - filename is sent over connectionQ to ftclient
*******************************************************************************/
void sendFile(int connectionP_FD, int connectionQ_FD, char* filename, char* clientIP, int clientPort, int hostPort){
  printf("File \"%s\" requested on port %d.\n", filename, hostPort);
  fflush(stdout);

  FILE* filePtr;
  // open the file to be sent in read/binary mode
  filePtr = fopen(filename, "r+b");
  // send an error message if the file cannot be sent
  if(filePtr == NULL) {
    printf("Requested file not found. Sending error message to %s:%d\n", clientIP, hostPort);
    fflush(stdout);
    char* errorMsg = "File not found\n";
    send(connectionP_FD, errorMsg, strlen(errorMsg), 0);
  }
  // otherwise send the file in chunks until it's all sent
  else {
    fseek(filePtr, 0, SEEK_END);
    long filesize = ftell(filePtr);
    printf("Sending \"%s\" (%ld Bytes) to %s:%d\n", filename, filesize, clientIP, clientPort);
    fflush(stdout);
    rewind(filePtr);
    if (filesize == EOF) {
      fprintf(stderr, "ERROR: file read error\n");
      exit(1);
    }
    if (filesize > 0) {
      do {
        size_t sendAmt = min(filesize, BUFFER_LENGTH);
        sendAmt = fread(buffer, 1, sendAmt, filePtr);
        int notSent = sendAmt;
        unsigned char* sendPtr = (unsigned char*)buffer;
        while (notSent > 0) {
          int sent = send(connectionQ_FD, sendPtr, notSent, 0);
          notSent -= sent;
          sendPtr += sent;
        }
        filesize -= sendAmt;
      }while(filesize > 0);
    }
    fclose(filePtr);
  }
}


void sendError(int connectionP_FD, char* message) { 
  int num_bytes = strlen(message);
  while (num_bytes > 0) {
    int sent = send(connectionP_FD, message, strlen(message), 0);
    num_bytes -= sent;
    message += sent;
  }
}

/*******************************************************************************
 *                void processClientRequest(int, char*)
 * Description: This is the jumping off point once a command has been received
 *   from ftclient. This function determines what the command is, connects
 *   on connection Q, determines what data should be sent back and begins the
 *   transfer accordingly
 * Input:
 *   int connectionP_FD - the file descriptor for connection P
 *   char* clientIP - the hostname or IP address that ftclient is running on
 *   int hostPort - the host port number for displaying status messages
 * Output: none
 * Preconditions: ftclient has connected on connection P and has sent a message
 *   string to ftserver
 * Postconditions: the rest of the actions dictated by the command sent from
 *   ftclient have been handled
*******************************************************************************/
void processClientRequest(int connectionP_FD, char* clientIP, int hostPort) {
  /* allocate memory for buffer, and create variables for command and file name */
  char* command;
  char* fileName;
  int charsRead,
      commandCode,
      clientPort,
      connectionQ_FD;

  memset(buffer, '\0', BUFFER_LENGTH);
  charsRead = recv(connectionP_FD, buffer, BUFFER_LENGTH, 0);
  if (charsRead <= 0) {
    exit(1);
  }
  parseCommands(buffer, &command, &fileName, &clientPort);
  connectionQ_FD = openConnectionQ(clientIP, clientPort);
  commandCode = getCommandCode(command);
  switch (commandCode) {
    case TRANSFER_CODE:
      sendFile(connectionP_FD, connectionQ_FD, fileName, clientIP, clientPort, hostPort);
      break;
    case LIST_CODE:
      sendDirectoryContents(connectionP_FD, connectionQ_FD, clientIP, clientPort, hostPort);
      break;
    case CD_CODE:
      printf("Change directory request received from %s:%d\n", clientIP, hostPort);
      fflush(stdout);
      if(chdir(fileName) != 0) {
        printf("Error switching to requested directory. Sending error message to %s:%d\n", clientIP, hostPort);
        fflush(stdout);
        sendError(connectionP_FD, "Error changing directory");
      }
      else {
        printf("Working directory changed to %s\n", fileName);
        fflush(stdout);
      }
      break;
    default:;
      char errorMsg[16] = "Invalid command";
      send(connectionP_FD, errorMsg, strlen(errorMsg), 0);
      break;
  }

  close(connectionP_FD);
  close(connectionQ_FD);
  
}
/*******************************************************************************
 *                    int main(int argc, char* argv[])
 * Description: this is the main function that runs ftserver. It takes a port 
 *   number passed in fromt he command line and opens a socket, listening on the
 *   supplied port number. When it recieves a connection, it runs the ft 
 *   protol as specified in Programming Assignment #2 CS372
 *   
 * Input:
 *   -argv[1] - the port number that ftserver will listen on for incoming
 *              connections
 * Output:
 *   - no output. The server transfers files over a TCP connection to a client
 *     program
 *   - returns 0 upon successful completion of the program
*******************************************************************************/
int main(int argc, char *argv[])
{
  /* variables */
  int listenSocketFD,
      connectionP_FD,
      portNumber;

  socklen_t sizeOfClientInfo;
  struct sockaddr_in serverAddress, clientAddress;
  char* clientIP;
  buffer = (char*)malloc(sizeof(char) * BUFFER_LENGTH);
  assert(buffer != NULL);

  /* validate argument and get port number*/
  validateArgs(argc, argv);
  setSignalHandler();
  portNumber = atoi(argv[1]);

  /* activate the socket */
  activateListenSocket(&listenSocketFD, &portNumber, &serverAddress);
  sizeOfClientInfo = sizeof(clientAddress);

  /* run the serer */
  while(TRUE) {
    
    printf("\nServer open on port %d\n", portNumber);
    fflush(stdout);
    connectionP_FD = accept(listenSocketFD, 
                                     (struct sockaddr*) &clientAddress,
                                     &sizeOfClientInfo);
    /* Source for getting clientIP address from an established connection
       https://stackoverflow.com/questions/4282369/determining-the-ip-address-
                                             of-a-connected-client-on-the-server
    */
    clientIP = inet_ntoa(clientAddress.sin_addr);
    printf("Connection from %s\n", clientIP);
    fflush(stdout);

    if(connectionP_FD < 0) {
      fprintf(stderr, "ERROR on accepting incoming connection.\n");
      exit(1);
    }
    else {
      processClientRequest(connectionP_FD, clientIP, portNumber);
    }
  }

  return 0;
}
