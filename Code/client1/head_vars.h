#define RCVBUFSIZE 1024 /* Size of receive buffer */

struct RegisterMsg
{
	char filename[32];
	int filesize;
	long int ipaddress;
	int portnum;
};

struct FileListReply
{
	char filename[32];
	int filesize;
};

struct FileSendFormat
{
	int packetno;
	int packetsize;
	char string[RCVBUFSIZE];
};

struct String
{
	char string[32];
};

struct ThreadArgs
{
	int contact_port;
	char *filename;
	int len_file;
	int peer_no;
	int no_of_peers;
};
