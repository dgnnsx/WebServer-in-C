#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define PORT 3000
#define MAXBUF 2048
#define GREEN	"\x1B[32m"
#define CYAN	"\x1B[36m"
#define YELLOW	"\x1B[33m"
#define RESET	"\x1B[0m"

#define OK 200
#define FORBIDDEN 403
#define NOT_FOUND 404

void header(int handler, int status) {
    char header[MAXBUF] = {0};
    if (status == OK) {
        sprintf(header, "HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Type: text/html; charset=UTF-8\r\n");
    } else if (status == FORBIDDEN) {
        sprintf(header, "HTTP/1.1 403 Forbidden\r\n\r\n");
    } else {
        sprintf(header, "HTTP/1.1 404 Not Found\r\nConnection: close\r\nContent-Type: text/html; charset=UTF-8\r\n");
    }
    send(handler, header, strlen(header), 0);
}

void resolve(int handler) {
    char buffer[MAXBUF];
    char * method, * filename;

    // The recv(), recvfrom(), and recvmsg() calls are used to receive messages from a socket
    // The recv() call is normally used only on a connected socket
    recv(handler, buffer, MAXBUF, 0);
    fprintf(stderr, "%s\n", buffer);
    method = strtok(buffer, " ");
    // Only accept GET requests
    if (strcmp(method, "GET") != 0) return;

    //  To signal strtok() that you want to keep searching the same string, you pass a NULL pointer as its first argument.
    // strtok() checks whether it is NULL and if it is so, it uses its currently stored data.
    // If the first parameter is not null, it is treated as a new search and all internal data is resetted.
    filename = strtok(NULL, " ");
    if (filename[0] == '/') filename++;

    if(filename[0] == '\0') {
        header(handler, OK);
        FILE * file = fopen("pages/index.html", "r");
        while (fgets(buffer, MAXBUF, file)) {
            send(handler, buffer, strlen(buffer), 0);
            memset(buffer, 0, MAXBUF);
        }
        return;
    }

    // access - determine accessibility of a file
    if (access(filename, F_OK) != 0) {
        header(handler, NOT_FOUND);
        FILE * file = fopen("pages/404.html", "r");
        while (fgets(buffer, MAXBUF, file)) {
            send(handler, buffer, strlen(buffer), 0);
            memset(buffer, 0, MAXBUF);
        }
        return;
    } else if (access(filename, R_OK) != 0) {
        header(handler, FORBIDDEN);
        FILE * file = fopen("pages/403.html", "r");
        while (fgets(buffer, MAXBUF, file)) {
            send(handler, buffer, strlen(buffer), 0);
            memset(buffer, 0, MAXBUF);
        }
        return;
    }
    header(handler, OK);
    FILE * file = fopen(filename, "r");
    while (fgets(buffer, MAXBUF, file)) {
        send(handler, buffer, strlen(buffer), 0);
        memset(buffer, 0, MAXBUF);
    }
}

int main(int argc, char **argv) {
    int server;
    struct sockaddr_in address;

    /*---Create streaming socket---*/
    if ((server = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

	/*---Initialize address/port structure---*/
	bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(PORT);
	address.sin_addr.s_addr = INADDR_ANY;

    /*---Assign a port number to the socket---*/
    if (bind(server, (struct sockaddr *) &address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

	/*---Make it a "listening socket"---*/
    if (listen(server, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    // accept incoming requests asynchronously
    int handler;
    socklen_t size;
    struct sockaddr_storage client;
	// Loop infinitly serving requests
	while(1) {
        size = sizeof(client);

        handler = accept(server, (struct sockaddr *) &client, &size);
        if(handler < 0) {
            perror("accept");
            continue;
        }

        resolve(handler);
		/*---Close data connection---*/
		close(handler);
	}
	/*---Clean up (should never get here!)---*/
	close(server);
	printf(YELLOW "\nServer down...\n\n" RESET);
    return EXIT_SUCCESS;
}
