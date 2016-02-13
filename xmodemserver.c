#include "xmodemserver.h"

#ifndef PORT
	#define PORT <53800>
#endif
#define WAITQUEUE_SIZE 5


/*
 * Create a socket, bind, and return listen file descriptor.
 */
int setupListen(void) {
	struct sockaddr_in addr;
	int socketfd;

	//Socket
	if ((socketfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("socket");
		exit(1);
	}

	//Bind
	memset(&addr, '\0', sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(PORT);
	if (bind(socketfd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
		perror("bind");
		exit(1);
	}

	//Listen
	if (listen(socketfd, WAITQUEUE_SIZE) == -1) {
		perror("listen");
		exit(1);
	}

	//Have OS release port ASAP on program termination
	if((setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int))) == -1) {
		perror("setsockopt");
	}
    
	return socketfd;
}


int main(void) {
	printf("Initializing Xmodem Server...\n");
	int listenFd = setupListen();//listening port
	printf("Initializing Client Management module...\n");
	initClientManagement(listenFd);//first time setup for clientmanagement.c
	printf("... ready\n\n");
	manageClients();

	return 0;
}