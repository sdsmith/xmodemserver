/*
 * Managing Connected Clients
 * 
 * ##########################################################
 * Error Checking
 * 
 * 	- detects and removes clients that have dropped from the server mid transfer
 *		- handled by readFromClient
 *	- if the amount being read from the client will overflow the buffer, drop the
 *		client
 *		- handled by readFromClient
 */
#include <sys/select.h>
#include "xmodemserver.h"


/*Function Prototypes*/
void initClientManagement(int listenFd);
void cleanClientFromServer(struct client *clnt);
void registerNewClient(void);
void dropClient(struct client *clnt);
int readFromClient(struct client *clnt, int nbytes);
void writeToClient(struct client *clnt, void *payload, int nbytes);
void writeToClientFile(struct client *clnt, void *payload, int nbytes);
int findNetworkNewline(char *buf);
char *extractLineFromBuffer(struct client *clnt, int newlineStartIndex);
char *extractFromBuffer(struct client *clnt, int nbytes);
void manageClients(void);

/*Global variables*/
struct client *clientList;			//linked-list of active clients
fd_set readClientFdSet;				//set of active read client fds
int largestClientFd;				//largest fd for active clients
int listenSocketFd;					//fd of socket server is using to listen


/*
 * Initializes the client management global varibles for use in file contained
 * functions. Must be called FIRST.
 */
void initClientManagement(int listenFd) {
	clientList = NULL;
	FD_ZERO(&readClientFdSet);
	listenSocketFd = listenFd;
	largestClientFd = listenSocketFd;
	FD_SET(listenSocketFd, &readClientFdSet);
}

/*
 * Update server values and remove server dependencies regarding client.
 */
void cleanClientFromServer(struct client *clnt) {
	//Update largest client file descriptor
	if (clnt->fd == largestClientFd) {
		largestClientFd = 0;
		if (listenSocketFd > largestClientFd) {//init largestClientFd
			largestClientFd = listenSocketFd;
		}
		struct client *curNode = clientList;

		while (curNode != NULL) {
			if (curNode->fd > largestClientFd) {
				largestClientFd = curNode->fd;
			}
			curNode = curNode->next;
		}
	}

	//close open files and fds
	if (close(clnt->fd) < 0) {
		perror("close");
		exit(1);
	}
	if (fclose(clnt->fp) != 0) {
		perror("close");
		exit(1);
	}
}

/*
 * Add a client to the client list and set of active file descriptors if one
 * is available on the given listen file descriptor. Also updates the largest
 * active client file descriptor.
 */
void registerNewClient(void) {
	struct sockaddr_in clientAddr;
	socklen_t sockLen = sizeof(clientAddr);
	int clientFd;

	if ((clientFd = accept(listenSocketFd, (struct sockaddr *)&clientAddr, &sockLen)) < 0) {
		perror("accept");
	} else {
		printf("Connection from %s on file descriptor %d\n", inet_ntoa(clientAddr.sin_addr), clientFd);

		struct client *clnt = malloc(sizeof(struct client));
		if (clnt == NULL) {
			perror("malloc"); 
			exit(1);
		}
	
		//set client default configuration	
		clnt->fd = clientFd;
		clnt->state = initial;	
		clnt->inbuf = 0;
		clnt->current_block = 1;
		memset(clnt->buf, '\0', 2048);

		if (clientList == NULL) {
			//no existing client
			clnt->next = NULL;
		} else {
			clnt->next = clientList;
		}
		
		clientList = clnt;
		FD_SET(clientFd, &readClientFdSet);

		//Update largest client file descriptor
		struct client *temp = clnt;
		while (temp != NULL) {
			if (clnt->fd > largestClientFd) {
				largestClientFd = clnt->fd;
			}
			temp = temp->next;
		}
	}
}

/*
 * Cleans server of client, removes client from the client list and delete 
 * client.
 */
void dropClient(struct client *clnt) {
	printf("Dropping client on file descriptor %d... \n", clnt->fd);
	struct client *curNode = clientList;
	int clientFd = clnt->fd;

	if (curNode != NULL) {
		//Clean-up the client
		cleanClientFromServer(clnt);
		
		//Remove client
		if (curNode->fd == clientFd) {//case head delete
			clientList = curNode->next;
			
			free(curNode);
			FD_CLR(clientFd, &readClientFdSet);
			printf("Client on file descriptor %d dropped\n", clientFd);
		} else {
			while (curNode->next != NULL) {
				if (curNode->next->fd == clientFd) {
					struct client *clntToDrop;
					clntToDrop = curNode->next;
					curNode->next = curNode->next->next;

					free(clntToDrop);//remove client info
					FD_CLR(clientFd, &readClientFdSet);//remove from active fds
					printf("Client on file descriptor %d dropped\n", clientFd);
					return;
				}
			}
		}
	}
}

/*
 * Reads information from the client and adds it to the client buffer. Returns
 * 0 if EOF (client disconnect) or number of bytes read otherwise. If a client
 * has disconnected (a return of 0), it will be auto dropped by this function.
 * If the data being read is greater than the remaining space in buffer, drop
 * client.
 */
int readFromClient(struct client *clnt, int nbytes) {
	int bytesRead;
	if (clnt->inbuf + nbytes > 2048) {
		//client's data will overflow buffer
		dropClient(clnt);
	} else if ((bytesRead = read(clnt->fd, clnt->buf + clnt->inbuf, nbytes)) == -1) {
		perror("read");
		dropClient(clnt);
	} else if (bytesRead == 0) {
		//client has disconnected (read found EOF)
		dropClient(clnt);
	}
	
	clnt->inbuf += bytesRead;	//update amount in buffer
	return bytesRead;
}

/*
 * Writes information to the given client.
 */
void writeToClient(struct client *clnt, void *payload, int nbytes) {
	if (write(clnt->fd, payload, nbytes) == -1) {
		perror("write");
		exit(1);
	}
}

/*
 * Writes payload to the file associated with the download of the given client
 */
void writeToClientFile(struct client *clnt, void *payload, int nbytes) {
	char *ptr = (char *)payload;
	int index = -1;
	//check for padding and zero it out
	while ((unsigned char)ptr[++index] != SUB && index < nbytes){};
	memset(ptr + index, '\0', nbytes - index);

	if (fwrite(payload, sizeof(char), index, clnt->fp) < index) {
		perror("write");
		exit(1);
	}
}

/*
 * Returns the index of the '\r' of the network new line ('\r\n'), -1 if it
 * does not exist.
 */
int findNetworkNewline(char *buf) {
	int index = 0;
	int newlineIndex = -1;

	while (newlineIndex == -1 && index < 2048) {
		if (buf[index] == '\r' && buf[index + 1] == '\n') {		
			newlineIndex = index;
		}
		index++;
	}
	return newlineIndex;
}

/*
 * Returns and removes the full line from the buffer and shifts the remaining
 * data to start of the buffer. Free pointer when done.
 */
char *extractLineFromBuffer(struct client *clnt, int newlineStartIndex) {
	char *buf = clnt->buf;
	char *fullLine = malloc(sizeof(char) * newlineStartIndex + 1);
	if (fullLine == NULL) {
		perror("malloc");
		exit(1);
	}
	
	strncpy(fullLine, buf, newlineStartIndex);//copy line w/o newline characters
	fullLine[newlineStartIndex] = '\0';//add string terminator
	clnt->inbuf -= newlineStartIndex + 2;//update num of items in buffer
	memset(buf, '\0', newlineStartIndex + 1);//erase full line from buffer
	memmove(buf, buf + newlineStartIndex + 2, clnt->inbuf);//mv to start of buf
	return fullLine;
}

/*
 * Extract a nbytes bytes from the buffer. Free pointer when done.
 */
char *extractFromBuffer(struct client *clnt, int nbytes) {
	char *buf = clnt->buf;
	char *data = malloc(nbytes);
	if (data == NULL) {
		perror("malloc");
		exit(1);
	}

	strncpy(data, buf, nbytes);
	clnt->inbuf -= nbytes;
	memset(buf, '\0', nbytes);
	memmove(buf, buf + nbytes, clnt->inbuf);
	return data;
}

/*
 * Reads from available connected clients and actions the appropriate procedure
 * dependent on the client's state.
 */
void manageClients(void) {
	fd_set readReadyClientSet = readClientFdSet;
	//int newClientFd;

	while (select(largestClientFd + 1, &readReadyClientSet, NULL, NULL, NULL) > -1) {
		//CCheck if listen fd is accept ready
		if (FD_ISSET(listenSocketFd, &readReadyClientSet)) {
			registerNewClient();
		}

		//Check if client fd is read ready
		struct client *curClient = clientList;
		while (curClient != NULL) {
			if (FD_ISSET(curClient->fd, &readReadyClientSet)) {
				switch(curClient->state) {
					case initial:
						clientState_initial(curClient);
						break;
					case pre_block:
						clientState_preBlock(curClient);
						break;
					case get_block:
						clientState_getBlock(curClient);
						break;
					default:
						break;
				}
			}
			curClient = curClient->next;
		}
		readReadyClientSet = readClientFdSet;
	}
	//Error has occured
	perror("select");
	exit(1);
}