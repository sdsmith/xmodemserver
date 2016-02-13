/*
 * Client state process
 * 
 * ##########################################################
 * Error Checking
 *
 *	- drops client if the filename will overflow the buffer
 *		- handled by clientState_initial
 *	- drop client if the block package sent by the client is not the size expected
 *		- handled by clientState_getBlock
 */
#include "xmodemserver.h"
#include "crc16.h"


//Function Prototypes
void clientState_initial(struct client *clnt);
void clientState_preBlock(struct client *clnt);
void clientState_getBlock(struct client *clnt);
void clientState_checkBlock(struct client *clnt);
void clientState_finished(struct client *clnt);


/*
 * Get filename from the client and setup file for transfer. Will drop client if
 * the filename will overflow the buffer.
 */
void clientState_initial(struct client *clnt) {
	printf("Starting transfer for client on file descriptor %d...\n", clnt->fd);
	//Waiting for filename
	int newlineStartIndex;
	if (readFromClient(clnt, 21 - clnt->inbuf)) {
		if ((newlineStartIndex = findNetworkNewline(clnt->buf)) != -1) {
			//Filename recieved, open file for writing and sending confirmation			
			char *filename = extractLineFromBuffer(clnt, newlineStartIndex);

			strncpy(clnt->filename, filename, strlen(filename));
			free(filename);
			clnt->fp = open_file_in_dir(clnt->filename, &(char[]){"filestore"}[0]);
			writeToClient(clnt, "C", 1);	//send confirmation to client
			clnt->state = pre_block;
		} else if (clnt->inbuf == 21) {//filename larger then buffer
			dropClient(clnt);
		}
	}
}

/*
 * Get info regarding upcoming block and setup
 */
void clientState_preBlock(struct client *clnt) {
	//Waiting for client information
	if (readFromClient(clnt, 1)) {
		if (clnt->inbuf >= 1) {			
			char *cntlChar = extractFromBuffer(clnt, 1);
		
			switch (*cntlChar) {
				case EOT:	//End Of Transfer
					//Send ACK, drop client
					writeToClient(clnt, &(char){ACK}, 1);
					clnt->state = finished;
					clientState_finished(clnt);
					break; 
				case SOH:	//128-byte payload
					clnt->blocksize = 128;
					clnt->state = get_block;
					break;
				case STX:	//1024-byte payload
					clnt->blocksize = 1024;
					clnt->state = get_block;
					break;
			}
			free(cntlChar);
		}
	}
}

/*
 * Get block from client.
 */
void clientState_getBlock(struct client *clnt) {
	//Wait for full block and advance to next state
	if (readFromClient(clnt, clnt->blocksize + 4 - clnt->inbuf)) {//+4 is block overhead
		if (clnt->inbuf == clnt->blocksize + 4) {//+4 is block overhead
			clnt->state = check_block;
			clientState_checkBlock(clnt);
		}
	} else if (clnt->inbuf > clnt->blocksize + 4) {//client sent larger packet then expected
		dropClient(clnt);
	}
}

/*
 * Check block from client.
 */
void clientState_checkBlock(struct client *clnt) {
	unsigned char *rawBlocknum;
	unsigned char *rawInverseBlocknum;
	char *payload;
	int blocknum;
	int inverseBlocknum;

	rawBlocknum = (unsigned char *)extractFromBuffer(clnt, 1);
	rawInverseBlocknum = (unsigned char *)extractFromBuffer(clnt, 1);
	blocknum = *rawBlocknum;
	inverseBlocknum = *rawInverseBlocknum;
	free(rawBlocknum); free(rawInverseBlocknum);

	if (inverseBlocknum != 255 - blocknum) {
		//block number and inverse don't match
		dropClient(clnt);
	} else if (blocknum + 1 == clnt->current_block) {
		//duplicate block recieved
		free(extractFromBuffer(clnt, clnt->blocksize + 2));//remove the junk data
		writeToClient(clnt, &(char){ACK}, 1);
		clnt->state = pre_block;
	} else if (blocknum == clnt->current_block) {
		//Valid block number
		unsigned short serverCrc16, clientCrc16;
		char *crcByte;
		payload = extractFromBuffer(clnt, clnt->blocksize);
		serverCrc16 = crc_message(XMODEM_KEY, (unsigned char *)payload, clnt->blocksize);
		
		//get the crc16 for the client
		crcByte = extractFromBuffer(clnt, 1);//high byte of clientCrc16
		clientCrc16 = *(unsigned short *)crcByte << 8; free(crcByte);
		crcByte = extractFromBuffer(clnt, 1);//low byte of clientCrc16
		clientCrc16 |= *(unsigned char *)crcByte; free(crcByte);

		if (serverCrc16 != clientCrc16) {
			//corrupted block
			writeToClient(clnt, &(char){NAK}, 1);
			clnt->state = pre_block;
		} else {
			//CRC16 match, block has been sucessfully recieved!
			writeToClient(clnt, &(char){ACK}, 1);				
			writeToClientFile(clnt, payload, clnt->blocksize);
			clnt->state = pre_block;

			//update expected block
			clnt->current_block++;
			if (clnt->current_block > 255) {
				clnt->current_block = 1;
			}
		}
		free(payload);
	} else {
		//Bad block number
		dropClient(clnt);
	}
}

/*
 * Drops client when it has successfully made a transfer.
 */
void clientState_finished(struct client *clnt) {
	printf("File transfer complete for client on file descriptor %d\n", clnt->fd);
	dropClient(clnt);
}