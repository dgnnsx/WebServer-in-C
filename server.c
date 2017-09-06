#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <fcntl.h>      /* Biblioteca para se utilizar a função open() */

#include <netdb.h>
#include <sys/socket.h>
#include <sys/stat.h>
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

#define CONNECTION_QUEUE       (1024)                /* Tamanho maximo da fila de conexoes pendentes */

/**
 * Estrutura utilizada para armazenar dados de um Host
 */
typedef struct {
    int socket;
    struct sockaddr_in address;
} Host;

/**
 * Estrutura utilizada para representar uma requisição de um cliente
 */
typedef struct {
    char buffer[MAXBUF];
    char * method;
    char * path;
    char * protocol;
} Request;

const char * get_mime_type(const char * name) {
    const char * ext = strrchr(name, '.');
    if (!ext) return NULL;
    if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0) return "text/html";
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(ext, ".gif") == 0) return "image/gif";
    if (strcmp(ext, ".png") == 0) return "image/png";
    if (strcmp(ext, ".css") == 0) return "text/css";
    if (strcmp(ext, ".au") == 0) return "audio/basic";
    if (strcmp(ext, ".wav") == 0) return "audio/wav";
    if (strcmp(ext, ".avi") == 0) return "video/x-msvideo";
    if (strcmp(ext, ".mpeg") == 0 || strcmp(ext, ".mpg") == 0) return "video/mpeg";
    if (strcmp(ext, ".mp3") == 0) return "audio/mpeg";
    return NULL;
}

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
    fprintf(stderr, "%s\n", header);
    send(handler, header, strlen(header), 0);
}

void sendPage(int handler, FILE * file) {
    char buffer[MAXBUF];
    while (fgets(buffer, MAXBUF, file)) {
        send(handler, buffer, strlen(buffer), 0);
        memset(buffer, 0, MAXBUF);
    }
    fclose(file);
}

void sendNotImplemented(int handler) {
    header(handler, 501, "Not Implemented", "text/html", -1);
    FILE * file = fopen("501.html", "r");
    sendPage(handler, file);
}

void sendNotFound(int handler) {
    header(handler, 404, "Not Found", "text/html", -1);
    FILE * file = fopen("404.html", "r");
    sendPage(handler, file);
}

void sendForbidden(int handler) {
    header(handler, 403, "Forbidden", "text/html", -1);
    FILE * file = fopen("403.html", "r");
    sendPage(handler, file);
}

void sendData(int handler, const char * path, struct stat statbuf) {

    int n = open(path, O_RDONLY);
    char dados;
    int i;

    FILE * file = fopen(path, "r");

    int tamanho = S_ISREG(statbuf.st_mode) ? statbuf.st_size : -1;
    header(handler, 200, "OK", get_mime_type(path), tamanho);
    // Enviando
    while ((i = read(n, &dados, 1)))
        write(handler, &dados, 1);
    //fclose(file);
}

Request readRequest(int handler) {
    Request request;
    // Colocar os dados do socket na variÃ¡vel request.buffer
    // The recv(), recvfrom(), and recvmsg() calls are used to receive messages from a socket
    // The recv() call is normally used only on a connected socket
    recv(handler, request.buffer, MAXBUF, 0);
    //  To signal strtok() that you want to keep searching the same string, you pass a NULL pointer as its first argument.
    // strtok() checks whether it is NULL and if it is so, it uses its currently stored data.
    // If the first parameter is not null, it is treated as a new search and all internal data is resetted.
    request.method = strtok(request.buffer, " ");
    request.path = strtok(NULL, " ");
    request.protocol = strtok(NULL, "\r");
    return request;
}

void resolve(int handler) {
    struct stat statbuf;

    Request clientRequest = readRequest(handler);
    if (!clientRequest.method || !clientRequest.path || !clientRequest.protocol) return;

    // Only accept GET requests
    if (strcasecmp(clientRequest.method, "GET") != 0) {
        sendNotImplemented(handler);
        return;
    } else if (stat(++clientRequest.path, &statbuf) < 0) {
        // Not Found
        fprintf(stderr, "%s\n", "erro");
        sendNotFound(handler);
    }

    //if (path[0] == '/') path++;
    /*if (path[0] == '\0') {
        sendData(handler, "index.html");
    } else if (access(path, F_OK) != 0) {
        // access - determine accessibility of a file

    } else if (access(path, R_OK) != 0) {
        // Forbidden
        sendForbidden(handler);
    } else {*/
        // OK
        sendData(handler, clientRequest.path, statbuf);
    //}
}

/**
 * Accept a new connection on a socket
 */
int acceptConnection(Host server) {
    int handler;
    struct sockaddr_storage client;
    socklen_t size = sizeof(client);
    handler = accept(server.socket, (struct sockaddr *) &client, &size);
    return handler;
}

/**
 * Listen for socket connections and limit the queue of incoming connections
 */
void listenSocket(int host) {
    if (listen(host, CONNECTION_QUEUE) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
}

/**
 * Bind a name to a socket
 */
void bindSocket(Host * host) {
    Host aux = *host;
    if (bind(aux.socket, (struct sockaddr *) &aux.address, sizeof(aux.address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    *host = aux;
}

/**
 * Create an endpoint for communication
 */
void createSocket(int * domain) {
    if ((*domain = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
}

Host createHost(int port) {
    Host host;

    /*---Create streaming socket---*/
    createSocket(&host.socket);

	/*---Initialize address/port structure---*/
	memset(&host.address, 0, sizeof(host.address));
    host.address.sin_family = AF_INET;
    host.address.sin_addr.s_addr = htonl(INADDR_ANY);
    host.address.sin_port = htons(port);

    /*---Assign a port number to the socket---*/
    bindSocket(&host);

    /*---Make it a "listening socket"---*/
    listenSocket(host.socket);
    return host;
}

int main(int argc, char **argv) {
    Host server = createHost(PORT);

    // Limpar a tela
    system("clear");
    printf(CYAN "\nHTTP server listening on port %d\n\n" RESET, PORT);

	// Loop infinitly serving requests
	while(1) {
        int handler = acceptConnection(server);
        if(handler < 0) {
            perror("accept");
            continue;
        }
        resolve(handler);
		/*---Close data connection---*/
		close(handler);
	}
	/*---Clean up (should never get here!)---*/
	close(server.socket);
	printf(YELLOW "\nServer down...\n\n" RESET);
    return EXIT_SUCCESS;
}
