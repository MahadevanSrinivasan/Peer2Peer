#define MAXPENDING 5/* Maximum outstanding connection requests */
#define MAXCLIENTS 5

void DieWithError(char *errorMessage);  /* Error handling function */
void FileSend(FILE *fp, int sock,int packet_no);
void MsgSendNum(int sock, int lenfile, int msgsize);
void RegisterClient(int sock, struct RegisterMsg msg);
int ComputeFileSize(FILE *fp);
int MsgReceiveNum(int sock);
void MsgReceiveString(int sock, char *returnstring);
void PrintFiles(int no_of_files, char passed_string[]);
void MsgSendString(int sock, struct String send_string);
void FileListMsg(int sock, struct FileListReply msg);
char* itoa(int val, int base);
void *ReceivePacketFromPeer(void *threadArgs);
void JoinFiles(int no_of_peers, char *filename);
void UnregisterPeer(int sock, int port_no);

int socketerrorflag=0,peerleft=0;
int peererror[MAXCLIENTS]={0,0,0,0,0};
void DieWithError(char *errorMessage)
{
	perror (errorMessage) ;
	//exit(1);
}

void FileSend(FILE *fp, int sock, int packet_no)
{
	struct FileSendFormat packet;
	int packet_count1,last_packet,buf_len,len_file=0,i=1;
	char *echoString;	/* String to send to echo server */

	rewind(fp);
	len_file = ComputeFileSize(fp);
	//MsgSendNum(sock, len_file, sizeof(len_file));	
	packet_count1 = len_file / RCVBUFSIZE;
	last_packet = len_file % RCVBUFSIZE;
	packet_count1 = (last_packet == 0) ? packet_count1 : packet_count1+1;
	last_packet = (last_packet == 0) ? RCVBUFSIZE : last_packet;		
	buf_len = (packet_count1 == packet_no+1) ? last_packet : RCVBUFSIZE;
	/* Skipping several strings to account for the packet number */
	for(i=0;i<packet_no;i++)
	{
		fread(packet.string,1,RCVBUFSIZE,fp);
	}
	fread(packet.string,1,buf_len,fp);
	packet.packetno = packet_no;
	packet.packetsize = buf_len;
	if (send(sock, &packet, sizeof(packet), 0) != sizeof(packet))
		DieWithError("send() sent a different number of bytes than expected");
	//printf("Sending Packet No. %d\n",packet_no);
}

void MsgSendNum(int sock, int len_file, int msgsize)
{
	if (send(sock, &len_file, msgsize, 0) != msgsize)
		DieWithError("send() sent a different number of bytes than expected");
}

void RegisterClient(int sock, struct RegisterMsg msg)
{
	if (send(sock, &msg, sizeof(msg), 0) != sizeof(msg))
		DieWithError("send() sent a different number of bytes than expected");	
}

int ComputeFileSize(FILE *fp)
{
	int len_file=0;	
	/* Compute size of the file in bytes */
	while(fgetc(fp)!=EOF)
		len_file++;
	rewind(fp);
	return(len_file);
}

int MsgReceiveNum(int sock)
{
	int reply;
	if(recv(sock, &reply, sizeof(reply), 0)<0)
			DieWithError("Receive () failed") ;
	return reply;
}

void MsgReceiveString(int sock, char *return_string)
{
	struct String send_string;
	memset(&send_string, 0, sizeof(send_string));
	if (recv(sock, &send_string, sizeof(send_string), 0) <0)
		DieWithError("Receive () failed");
	strcpy(return_string,send_string.string);
}

void PrintFiles(int no_of_files, char passed_string[])
{
	int i=0,j=0,k=0;
	char temp_string[32];
	for(j=0;j<strlen(passed_string);j++)
	{
		if(passed_string[j] == '\n')
		{
			temp_string[k]='\0';
			printf("%d. %s\n",i+1,temp_string);
			k=0;
			i++;
			memset(&temp_string, 0, sizeof(temp_string));
		}
		else
		{
			temp_string[k] = passed_string[j];
			k++;
		}
	}
}

void MsgSendString(int sock, struct String send_string)
{
	if (send(sock, &send_string, sizeof(send_string), 0) != sizeof(send_string))
		DieWithError("send() sent a different number of bytes than expected");
}

void FileListMsg(int sock, struct FileListReply msg)
{
	if (send(sock, &msg, sizeof(msg), 0) != sizeof(msg))
		DieWithError("send() sent a different number of bytes than expected");	
}

void *ReceivePacketFromPeer(void *threadArgs)
{
	int contact_port, len_file, peer_no, no_of_peers, start_packet=0,count=0;
	char *filename;
	int req_sock,packet_count,last_packet,incre_packet_count=0,total_packet_count=0;
	FILE *fp;
	struct sockaddr_in req_sock_addr;
	struct String send_string;
	struct FileSendFormat packet;
	char *filename1;

	contact_port = ((struct ThreadArgs *)threadArgs) -> contact_port;
	filename = ((struct ThreadArgs *)threadArgs) -> filename;
	len_file = ((struct ThreadArgs *)threadArgs) -> len_file;
	peer_no = ((struct ThreadArgs *)threadArgs) -> peer_no;
	no_of_peers = ((struct ThreadArgs *)threadArgs) -> no_of_peers;
	
	free(threadArgs);
	packet_count = len_file / RCVBUFSIZE;
	last_packet = len_file % RCVBUFSIZE;
	packet_count = (last_packet == 0) ? packet_count : packet_count+1;
	last_packet = (last_packet == 0) ? RCVBUFSIZE : last_packet;
	/* Request Socket Definition */
	memset(&req_sock_addr, 0, sizeof(req_sock_addr)); /* Zero out structure */
	req_sock_addr.sin_family = AF_INET; /* Internet address family */
	req_sock_addr.sin_addr.s_addr = htonl(INADDR_ANY); /* Any incoming interface */
	req_sock_addr.sin_port = htons(contact_port); /*MsgSendNum Local port */			
	
	if ((req_sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
		DieWithError( "socket () failed") ;		
	
	if (connect(req_sock, (struct sockaddr *) &req_sock_addr, sizeof(req_sock_addr)) < 0)
		DieWithError("connect () failed") ;
	
	MsgSendNum(req_sock, 1, sizeof(int));
	memset(&send_string, 0, sizeof(send_string));
	strcpy(send_string.string,filename);
	MsgSendString(req_sock,send_string);
	memset(&filename1,0,sizeof(filename1));
	

	total_packet_count = packet_count;
	packet_count = (int)(packet_count/no_of_peers);
	start_packet = (peer_no * packet_count);
	if(peer_no == no_of_peers-1)
		packet_count = total_packet_count - (packet_count*(no_of_peers-1));


	incre_packet_count = start_packet;

	MsgSendNum(req_sock, start_packet, sizeof(int));
	MsgSendNum(req_sock, packet_count, sizeof(int));
	filename1 = malloc((strlen(filename)*sizeof(char))+2);
	strcpy(filename1,filename);
	strcat(filename1,"_");
	strcat(filename1,itoa(peer_no,10));
	fp = fopen(filename1,"w");
	printf("Receiving Data from Peer at port : %d\n",contact_port);
	while(packet_count)
	{
		MsgSendNum(req_sock,incre_packet_count,sizeof(int));	
		if ((recv(req_sock, &packet, sizeof(packet), 0)) < 0)
			DieWithError("recv() failed") ;
		fwrite(packet.string,1,packet.packetsize,fp);
		packet_count--;
		count++;
		incre_packet_count++;
		//if(peer_no == 0)
		{
			printf("Download Progress : %d \n",((count*no_of_peers*100) / total_packet_count));
		}
		if(socketerrorflag == 1)
		{
			peerleft = 1;
			socketerrorflag = 0;
			printf("There is a problem with peer no. %d\n",peer_no);
			peererror[peer_no] = 1;
			break;
		}
	}
	fclose(fp);
	close(req_sock);

}

char* itoa(int val, int base)
{
	
	static char buf[32] = {0};
	
	int i = 30;
	
	for(; val && i ; --i, val /= base)
	
		buf[i] = "0123456789abcdef"[val % base];
	
	return &buf[i+1];
	
}

void JoinFiles(int no_of_peers, char *filename)
{
	int i=0;
	FILE *fpread,*fpwrite;
	char *filename1,ch,*filename2;
	
	filename1 = malloc((strlen(filename)*sizeof(char))+4);
	filename2 = malloc((strlen(filename)*sizeof(char))+7);
	strcpy(filename1,filename);
	//strcat(filename1,"_rec");
	fpwrite = fopen(filename1,"w");

	for(i=0;i<no_of_peers;i++)
	{
		strcpy(filename1,filename);
		strcat(filename1,"_");
		strcat(filename1,itoa(i,10));
		fpread = fopen(filename1,"r");
		while((fread(&ch,1,1,fpread)))
		{
			fwrite(&ch,1,1,fpwrite);
		}
		fclose(fpread);
		strcpy(filename2,"rm ");
		strcat(filename2,filename1);
		system(filename2);
		
	}
	fclose(fpwrite);
	printf("Received & Merged Files : TADA!\n");
}

void UnregisterPeer(int sock, int port_no)
{
	MsgSendNum(sock,4,sizeof(int));
	MsgSendNum(sock,port_no,sizeof(int));
	close(sock);
	printf("Unregistering the misbehaving peer from server\n");
}
