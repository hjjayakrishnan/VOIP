#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#define NSTRS 3

char *strs[NSTRS] = {
	"This is the first server string.\n",
	"This is the second server string.\n",
	"This is the third server string.\n"
};

extern int errno;
extern void int_handler();
extern void broken_pipe_handler();
extern void serve_clients();

static int server_sock, client_sock;
static int fromlen, i, j, num_sets;
static char c;
static FILE *fp;


static struct sockaddr_in server_sockaddr, client_sockaddr;

int main(int argc, char **argv)
{
	
	char hostname[64];
	struct hostent *hp;
    struct linger opt;
    int sockarg;

	//gethostname(hostname, sizeof(hostname));

	if((hp = (struct hostent*) gethostbyname(argv[1])) == NULL) {
		fprintf(stderr, "%s: host unknown.\n", hostname);
		exit(1);
	}

	if((server_sock=socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("server: socket");
		exit(1);
	}

	bzero((char*) &server_sockaddr, sizeof(server_sockaddr));
	server_sockaddr.sin_family = AF_INET;
	server_sockaddr.sin_port = htons(1234);
	bcopy (hp->h_addr, &server_sockaddr.sin_addr, hp->h_length);

	/* Bind address to the socket */
	if(bind(server_sock, (struct sockaddr *) &server_sockaddr,
            sizeof(server_sockaddr)) < 0) 
    {
		perror("server: bind");
		exit(1);
	}

    /* turn on zero linger time so that undelivered data is discarded when
       socket is closed
     */
    opt.l_onoff = 1;
    opt.l_linger = 0;

    sockarg = 1;
 
    setsockopt(server_sock, SOL_SOCKET, SO_LINGER, (char*) &opt, sizeof(opt));
    setsockopt(client_sock, SOL_SOCKET, SO_REUSEADDR, (char *)&sockarg, sizeof(int));
	signal(SIGINT, int_handler);
	signal(SIGPIPE, broken_pipe_handler);

	serve_clients();

return 0;
}

/* Listen and accept loop function */
void serve_clients()
{
	char fileToWriteOut_Ptr[]="Richard Noronha\n";

	int lengthOfFile;
	for(;;) {
		/* Listen on the socket */
		if(listen(server_sock, 5) < 0) {
			perror("server: listen");
			exit(1);
		}
                else
                {
                    printf("Listening to port 1234\n");
                }

		/* Accept connections */
		if((client_sock=accept(server_sock, 
                               (struct sockaddr *)&client_sockaddr,
                               &fromlen)) < 0) 
        {
			perror("server: accept");
			exit(1);
		}
                else
                {
                    printf("Connection made on port 1234\n");
                }

		fp = fdopen(client_sock, "r");

		printf("Size of file: %d\n",sizeof(fileToWriteOut_Ptr));
		//printf("Size of int: %d\n",sizeof(sizeof(*fileToWriteOut_Ptr)));
		lengthOfFile=sizeof(fileToWriteOut_Ptr);
		send(client_sock, (char *)(&lengthOfFile), sizeof(int), 0);		//This sends to the server the number of sets that the client has and wishes to transfer. Thats why size of int
for(int i=0;i<1000;i++);		//test delay function not needed on Jetson
		
		
		send(client_sock, (char *)(fileToWriteOut_Ptr), lengthOfFile, 0);

		close(client_sock);

	}

}

/* Close sockets after a Ctrl-C interrupt */
void int_handler()
{
	char ch;

	printf("Enter y to close sockets or n to keep open:");
	scanf("%c", &ch);

	if(ch == 'y') {
		printf("\nsockets are being closed\n");
		close(client_sock);
		close(server_sock);
	}

	exit(0);

}

void broken_pipe_handler()
{
	char ch;

	printf("Enter y to continue serving clients or n to halt:");
	scanf("%c", &ch);

	if(ch == 'y') {
		printf("\nwill continue serving clients\n");
		serve_clients();
	}
	else
		exit(0);

}

