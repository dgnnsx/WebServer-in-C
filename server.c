#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define PORT 3000
#define MAXBUF 4096
#define GREEN	"\x1B[32m"
#define CYAN	"\x1B[36m"
#define YELLOW	"\x1B[33m"
#define RESET	"\x1B[0m"

#define PROTOCOL "HTTP/1.0"
#define SERVER "Webserver in C"
#define RFC1123FMT "%a, %d %b %Y %H:%M:%S GMT"

void header(int handler, int status, const char * title, const char * mimeType, int length) {
    char header[MAXBUF] = {0};
    char timebuf[128] = {0};
    char aux[128] = {0};
    time_t now;

    sprintf(aux, "%s %d %s\r\n", PROTOCOL, status, title);
    strcat(header, aux);

    memset(aux, '\0', sizeof(aux));
    sprintf(aux, "Server: %s\r\n", SERVER);
    strcat(header, aux);

    now = time(NULL);
    strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&now));
    memset(aux, '\0', sizeof(aux));
    sprintf(aux, "Date: %s\r\n", timebuf);
    strcat(header, aux);

    if (mimeType) {
        memset(aux, '\0', sizeof(aux));
        sprintf(aux, "Content-Type: %s\r\n", mimeType);
        strcat(header, aux);
    }
    if (length >= 0) {
        memset(aux, '\0', sizeof(aux));
        sprintf(aux, "Content-Length: %d\r\n", length);
        strcat(header, aux);
    }
    strcat(header, "Connection: close\r\n\r\n");
    send(handler, header, strlen(header), 0);
}

void sendFile(int handler, FILE * file) {
    char buffer[MAXBUF];
    while (fgets(buffer, MAXBUF, file)) {
        send(handler, buffer, strlen(buffer), 0);
        memset(buffer, 0, MAXBUF);
    }
}

void sendNotImplemented(int handler) {
    header(handler, 501, "Not Implemented", "text/html", -1);
    FILE * file = fopen("./pages/501.html", "r");
    sendFile(handler, file);
}

void sendNotFound(int handler) {
    header(handler, 404, "Not Found", "text/html", -1);
    FILE * file = fopen("./pages/404.html", "r");
    sendFile(handler, file);
}

void sendForbidden(int handler) {
    header(handler, 403, "Forbidden", "text/html", -1);
    FILE * file = fopen("./pages/403.html", "r");
    sendFile(handler, file);
}

void sendPage(int handler, const char * path) {
    header(handler, 200, "OK", "text/html", -1);
    FILE * file = fopen(path, "r");
    sendFile(handler, file);
}

void resolve(int handler) {
    char buffer[MAXBUF];
    char * method;
    char * path;
    char * protocol;

    // The recv(), recvfrom(), and recvmsg() calls are used to receive messages from a socket
    // The recv() call is normally used only on a connected socket
    recv(handler, buffer, MAXBUF, 0);
    fprintf(stderr, "%s\n", buffer);

    //  To signal strtok() that you want to keep searching the same string, you pass a NULL pointer as its first argument.
    // strtok() checks whether it is NULL and if it is so, it uses its currently stored data.
    // If the first parameter is not null, it is treated as a new search and all internal data is resetted.
    method = strtok(buffer, " ");
    path = strtok(NULL, " ");
    protocol = strtok(NULL, "\r");

    if (!method || !path || !protocol) return;

    // Only accept GET requests
    if (strcasecmp(method, "GET") != 0) {
        sendNotImplemented(handler);
        return;
    }

    if (path[0] == '/') path++;
    if (path[0] == '\0') {
        sendPage(handler, "./pages/index.html");
    } else if (access(path, F_OK) != 0) {
        // access - determine accessibility of a file
        // Not Found
        sendNotFound(handler);
    } else if (access(path, R_OK) != 0) {
        // Forbidden
        sendForbidden(handler);
    } else {
        // OK
        sendPage(handler, path);
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
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

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
    printf("\nHTTP server listening on port %d\n\n", PORT);

	// Loop infinitly serving requests
	while(1) {
        int handler;
        socklen_t size;
        struct sockaddr_storage client;

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
