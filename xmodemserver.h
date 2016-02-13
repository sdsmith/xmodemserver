/* This header file should be included in your xmodemserver.c file.
 * You are welcome to add to it.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>

/* Enumeration constants are similar to constants created with #define
 * It is a nice way of being able to give a name to a state in this case.
 * For example, you can use the enum below as follows:
 *     //first declare a variable:
 *     enum recstate state;
 *     //then give it a value (note that the value is not a string)
 *     state = check_block;
 */
enum recstate {
	initial,
	pre_block,
	get_block,
	check_block,
	finished
};

/* The client struct contains the state that the server needs for each client.
 */
struct client {
	int fd;               // socket descriptor for this client
	char buf[2048];       // buffer to hold data being read from client
	int inbuf;            // index into buf
	char filename[20];    // name of the file being transferred
	FILE *fp;             // file pointer for where the file is written to
	enum recstate state;  // current state of data transfer for this client
	int blocksize;        // the size of the current block
	int current_block;    // the block number of the current block
	struct client *next;  // a pointer to the next client in the list
};

#define XMODEM_KEY 0x1021


//ASCII characters
#define SOH 1
#define STX 2
#define EOT 4
#define ACK 6
#define NAK 21
#define CAN 24
#define SUB 26


//helper.c
FILE *open_file_in_dir(char *filename, char *dirname);

//clientmanagment.c
void initClientManagement(int listenFd);
void registerNewClient(void);
void dropClient(struct client *clnt);
int readFromClient(struct client *clnt, int nbytes);
void writeToClient(struct client *clnt, void *payload, int nbytes);
void writeToClientFile(struct client *clnt, void *payload, int nbytes);
int findNetworkNewline(char *buf);
char *extractLineFromBuffer(struct client *clnt, int newlineStartIndex);
char *extractFromBuffer(struct client *clnt, int nbytes);
void manageClients(void);

//clientstateprocess.c
void clientState_initial(struct client *clnt);
void clientState_preBlock(struct client *clnt);
void clientState_getBlock(struct client *clnt);
void clientState_checkBlock(struct client *clnt);
void clientState_finished(struct client *clnt);