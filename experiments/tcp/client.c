#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#define NSTRS 3
#define MAX_IT 3
#define LOCAL_PORT 1234

char *strs[NSTRS] = {
	"Jack be nimble.\n",
	"Jack be quick.\n",
	"Jack jump over the candlestick.\n"
};


extern int errno;
extern void broken_pipe_handler();

int main(argc, argv)
int argc;
char **argv;
{
    char c;
    FILE *fp;
	
	char fileToRead_Ptr[50];

    char hostname[64];
    int i, j, client_sock, len, num_sets;
    struct hostent *hp;
    struct sockaddr_in client_sockaddr;
    struct linger opt;
    int sockarg;
    unsigned short port;
 
	//gethostname(hostname, sizeof(hostname));

	if((hp = gethostbyname(argv[1])) == NULL) {
		fprintf(stderr, "%s: unknown host.\n", hostname);
		exit(1);
	}
        else
        {
            printf("Connected to %s\n", argv[1]);
        }

	if((client_sock=socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("client: socket");
		exit(1);
	}

	client_sockaddr.sin_family = AF_INET;
        sscanf(argv[2], "%u", &port);
        printf("Will connect to port=%u\n", port);
	client_sockaddr.sin_port = htons(port);
	//client_sockaddr.sin_port = htons(LOCAL_PORT);
	bcopy(hp->h_addr, &client_sockaddr.sin_addr, hp->h_length);

    /* discard undelivered data on closed socket */ 
    opt.l_onoff = 1;
    opt.l_linger = 0;

    sockarg = 1;

    setsockopt(client_sock, SOL_SOCKET, SO_LINGER, (char*) &opt, sizeof(opt));

    setsockopt(client_sock, SOL_SOCKET, SO_REUSEADDR, (char *)&sockarg, sizeof(int));

    if(connect(client_sock, (struct sockaddr*)&client_sockaddr, sizeof(client_sockaddr)) < 0) 
    {
	perror("client: connect");
	exit(1);
    }
    else
    {
        printf("CONNECTED TO REMOTE SERVER\n");
    }


	signal(SIGPIPE, broken_pipe_handler);

	fp = fdopen(client_sock, "r");
	
	recv(client_sock, (char *)&num_sets, sizeof(int), 0);
	
	printf("number of sets = %d\n", num_sets);

	//recv(client_sock, (char *)&fileToRead_Ptr, num_sets, 0);
	/*for(i=0;i<15;i++)
		printf("%c",fileToRead_Ptr[i]);*/
	int count=0;
	while(count<num_sets) {			//prints the characters that were transmitted
		count++;
					c = fgetc(fp);			//use this for obtaining individial characters from the file pointer
						putchar(c);

					//if(c=='\n')
						//break;
				} /* end while */
	close(client_sock);

	exit(0);
return 0;
}

void broken_pipe_handler()
{

	printf("\nbroken pipe signal received\n");

}
