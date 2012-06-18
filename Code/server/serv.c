#include <stdio.h> /* for printf() and fprintf() */
#include <sys/socket.h> /* for socket(), bind(), and connect() */
#include <arpa/inet.h> /* for sockaddr_in and inet_ntoa() */
#include <stdlib.h>/* for atoi() */
#include <string.h>/* for memset() */
#include <unistd.h>/* for close() */
#include <math.h>
#include "head_vars.h"
#include "headfun.h"

/* Thread to handle the clients */
void ClientThread(void *threadClientArgs);

/* Data base to store the registered files */
struct RegisterMsg **clients = NULL;
struct RegisterMsg **clients_temp = NULL;
/* No. of files registered */
int no_of_reg_files=0,temp_reg_files=0;

/* Parameter to be passed to the thread */
struct ThreadClientArgs
{
	int clntSock;
};
	

int main(int argc, char *argv[])
{
	int servSock,clntSock; /* Socket descriptor for server */
	struct sockaddr_in echoServAddr; /* Local address */
	struct sockaddr_in echoClntAddr; /* Client address */
	unsigned short echoServPort; /* Server port */
	unsigned int clntLen; /* Length of client address data structure */
	pthread_t threadID;
	int returnValue;

	if (argc != 2) /* Test for correct number of arguments */
	{
		fprintf(stderr, "Usage: %s <Server Port>\n", argv[0]) ;
		exit(1);
	}
	echoServPort = atoi(argv[1]); /* First arg: local port */

	/* Create socket for incoming connections */

	if ((servSock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
		DieWithError( "socket () failed") ;
		
	/* Construct local address structure */
	memset(&echoServAddr, 0, sizeof(echoServAddr)); /* Zero out structure */
	echoServAddr.sin_family = AF_INET; /* Internet address family */
	echoServAddr.sin_addr.s_addr = htonl(INADDR_ANY); /* Any incoming interface */
	echoServAddr.sin_port = htons(echoServPort); /*MsgSendNum Local port */
	/* Bind to the local address */
	if (bind(servSock, (struct sockaddr *)&echoServAddr, sizeof(echoServAddr)) < 0)
		DieWithError ( "bind () failed");
	/* Mark the socket so it will listen for incoming connections */
	if (listen(servSock, MAXPENDING) < 0)
		DieWithError("listen() failed") ;


	for (;;) /* Run forever */
	{
		/* Set the size of the in-out parameter */
		clntLen = sizeof(echoClntAddr);
		/* Wait for a client to connect */
		if ((clntSock = accept(servSock, (struct sockaddr *) &echoClntAddr,
		&clntLen)) < 0)
			DieWithError("accept() failed");
		/* clntSock is connected to a client! */
		struct ThreadClientArgs *threadClientArgs = (struct ThreadClientArgs *) malloc(
		sizeof(struct ThreadClientArgs));
		if (threadClientArgs == NULL)
			DieWithError("malloc() failed");
		
		/* Pass the client's socket information to the thread */
		threadClientArgs->clntSock = clntSock;
		
		/* Create client thread */
		returnValue = pthread_create(&threadID, NULL, ClientThread, threadClientArgs);
		if (returnValue != 0)
			DieWithError("pthread_create() failed");		
	}
	/* Never reached */
	close(clntSock); /* Close client socket */
}

void ClientThread(void *threadClientArgs)
{
	int len_file,
		packet_count,
		last_packet,
		count=0,
		file_id,
		no_of_files,
		msg_id,
		i,
		contact_port,
		recvMsgSize,
		register_break;
	int clntSock;
	struct RegisterMsg msg;
	struct FileListReply list_of_files;
	
	/* Extract socket file descriptor from argument */
	clntSock = ((struct ThreadClientArgs *) threadClientArgs)->clntSock;
	free(threadClientArgs); /* Deallocate memory for argument */
	
	msg_id=MsgReceiveNum(clntSock);

	switch(msg_id)
	{
	case 1:
	{
		/* Register Files command */
		printf("Request for registering files received.\n");
		no_of_files = MsgReceiveNum(clntSock);
		printf("No. of files to be registered = %d\n",no_of_files);
		for(i=0;i<no_of_files;i++)
		{
		register_break = MsgReceiveNum(clntSock);
		if(register_break == 0)
		{
			printf("Register files aborted\n");
			printf("No. of registered files = %d\n",no_of_reg_files);
			break;
		}
		/* Receive the file information structure */
		if ((recvMsgSize = recv(clntSock, &msg, sizeof(msg), 0)) < 0)
				DieWithError("recv() failed") ;
		clients = (struct RegisterMsg **)realloc(clients,(no_of_reg_files+1)*sizeof(struct RegisterMsg *));
		clients[no_of_reg_files] = (struct RegisterMsg *)malloc(sizeof(struct RegisterMsg));
		strcpy(clients[no_of_reg_files]->filename,msg.filename);
		clients[no_of_reg_files]->filesize=msg.filesize;
		clients[no_of_reg_files]->ipaddress=msg.ipaddress;
		clients[no_of_reg_files]->portnum=msg.portnum;
				
		printf("Registered File : %s of size %d bytes from Port %d\n",clients[no_of_reg_files]->filename,clients[no_of_reg_files]->filesize,ntohs(clients[no_of_reg_files]->portnum));
		no_of_reg_files++;
		system("clear");
		printf("No. of registered files = %d\n",no_of_reg_files);
		MsgSendNum(clntSock,1,sizeof(int));
		}
		break;
	}
	case 2:
	{
		/* File List Request command */
		printf("File List Request received.\n");
		memset(&list_of_files, 0, sizeof(list_of_files)); /* Zero out structure */
		/* Send the number of files registered */
		MsgSendNum(clntSock,no_of_reg_files,sizeof(int));
		/* Send the file information */
		for(i=0;i<no_of_reg_files;i++)
		{
			list_of_files.filesize = clients[i]->filesize;
			strcpy(list_of_files.filename,clients[i]->filename);
			MsgSendNum(clntSock,sizeof(list_of_files),sizeof(int));
			FileListMsg(clntSock,list_of_files);
		}
		break;
	}
	case 3:
	{
		/* File Location Request command */
		printf("File Location Request received.\n");
		MsgSendNum(clntSock,no_of_reg_files,sizeof(int));
		file_id=MsgReceiveNum(clntSock);
		/* Error handling */
		if(file_id > no_of_reg_files || file_id < 1)
			break;
		printf("Requested for the details of file : %d\n",file_id);
		count = 0;
		for(i=0;i<no_of_reg_files;i++)
		{

			if(!(strcmp(clients[file_id-1]->filename, clients[i]->filename)))
				count++;
		}
		memset(&msg, 0, sizeof(msg)); /* Zero out structure */
		MsgSendNum(clntSock,count,sizeof(int));
		/* Send information of all the peers which have the file */
		for(i=0;i<no_of_reg_files;i++)
		{
			if(!(strcmp(clients[file_id-1]->filename, clients[i]->filename)))
			{
				strcpy(msg.filename,clients[i]->filename);
				msg.filesize = clients[i]->filesize;
				msg.ipaddress = clients[i]->ipaddress;
				msg.portnum = clients[i]->portnum;
				printf("%d. %s\n",i+1,msg.filename);
				RegisterClient(clntSock,msg);
			} 
				
		}
		break;
	}
	case 4:
	{
		/* Leave Request command */
		contact_port = MsgReceiveNum(clntSock);
		count = 0;
		
		/* Copy the database onto a new one leaving out the peer requested the leave */

		for(i=0;i<no_of_reg_files;i++)
		{
			if(ntohs(clients[i] -> portnum) != contact_port)
			{
				clients_temp = (struct RegisterMsg **)realloc(clients_temp,(temp_reg_files+1)*sizeof(struct RegisterMsg *));
				clients_temp[temp_reg_files] = (struct RegisterMsg *)malloc(sizeof(struct RegisterMsg));
				strcpy(clients_temp[temp_reg_files] -> filename,  clients[i] -> filename);
				clients_temp[temp_reg_files] -> filesize =  clients[i] -> filesize;
				clients_temp[temp_reg_files] -> ipaddress =  clients[i] -> ipaddress;
				clients_temp[temp_reg_files] -> portnum =  clients[i] -> portnum;
				temp_reg_files++;
				printf("Retaining entry %d\n",i+1);
			}
		}

		/* Free as many entries as required */
		for(i=temp_reg_files;i<no_of_reg_files;i++)
		{
			free(clients[i]);
		}

		/* Copy back into the original structure */
		for(i=0;i<temp_reg_files;i++)
		{
			strcpy(clients[i] -> filename,clients_temp[i] -> filename);
			clients[i] -> filesize = clients_temp[i] -> filesize;
			clients[i] -> ipaddress = clients_temp[i] -> ipaddress;
			clients[i] -> portnum = clients_temp[i] -> portnum;
		}

		/* Adjust the number of files */
		no_of_reg_files = temp_reg_files;

		/* Free all the elements in clients_temp */
		for(i=0;i<temp_reg_files;i++)
		{
			free(clients_temp[i]);
		}
		temp_reg_files=0;
		system("clear");
 		printf("No.of Registered Files = %d\n",no_of_reg_files);
		/* Print the files registered with the server */
		for(i=0;i<no_of_reg_files;i++)
		{
			printf("%d\t%s\t%d\n",i+1,clients[i]->filename,clients[i]->filesize);
		}
		break;

	}
	}
		//return(NULL);

}

