#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>                                      /* Biblioteca para se utilizar a função open() */
#include <netdb.h>
#include <pthread.h>                                    /* Biblioteca para utilização de THREADS */
#include <sys/socket.h>                                 /* Biblioteca contendo as definições de sockets*/
#include <sys/stat.h>                                   /* Biblioteca para se utilizar a struct stat para verificação de arquivos */
#include <arpa/inet.h>                                  /* Biblioteca contendo Funcões referentes ao inet (Rede) */

#define PORT 3000
#define MAXBUF 4096
#define GREEN	"\x1B[32m"
#define CYAN	"\x1B[36m"
#define YELLOW	"\x1B[33m"
#define RESET	"\x1B[0m"

#define FAILURE -1
#define SUCCESS 1

#define PROTOCOL "HTTP/1.0"
#define SERVER "Webserver in C"
#define RFC1123FMT "%a, %d %b %Y %H:%M:%S GMT"
#define MAX_THREAD  20                                  /* Tamanho maximo de threads*/
#define CONNECTION_QUEUE       (1024)                   /* Tamanho maximo da fila de conexoes pendentes */

/**
 * Estrutura utilizada para armazenar dados de um Host
 */
typedef struct {
    int socket;
    struct sockaddr_in address;
} Host;

/**
 * Estrutura para armazenar servidor e cliente
 */
typedef struct {
    Host server;
    Host client;
} Hosts;

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
    char dados;
    int i;
    int n = open(path, O_RDONLY);
    size_t tamanho = S_ISREG(statbuf.st_mode) ? statbuf.st_size : -1;
    header(handler, 200, "OK", get_mime_type(path), tamanho);
    // Enviando
    while ((i = read(n, &dados, 1)))
        write(handler, &dados, 1);
}

Request readRequest(int handler) {
    Request request;
    // Colocar os dados do socket na variÃ¡vel request.buffer
    // The recv(), recvfrom(), and recvmsg() calls are used to receive messages from a socket
    // The recv() call is normally used only on a connected socket
    //recv(handler, request.buffer, MAXBUF, 0);
    printf("\n\nREADING REQUEST\n");
    read(handler, request.buffer, sizeof(request.buffer));
    //  To signal strtok() that you want to keep searching the same string, you pass a NULL pointer as its first argument.
    // strtok() checks whether it is NULL and if it is so, it uses its currently stored data.
    // If the first parameter is not null, it is treated as a new search and all internal data is resetted.
    printf("%s\n", request.buffer);
    request.method = strtok(request.buffer, " ");
    request.path = strtok(NULL, " ");
    request.protocol = strtok(NULL, "\r");
    return request;
}

//void resolve(int handler) {
void resolve(Host server, Host client) {
    struct stat statbuf;

    Request clientRequest = readRequest(client.socket);
    if (!clientRequest.method || !clientRequest.path || !clientRequest.protocol) return;

    // Only accept GET requests
    if (strcasecmp(clientRequest.method, "GET") != 0) {
        sendNotImplemented(client.socket);
        return;
    }

    if (clientRequest.path[0] == '/') clientRequest.path++;
    if (clientRequest.path[0] == '\0') clientRequest.path = (char *) "index.html";

    if (stat(clientRequest.path, &statbuf) < 0) {
        // Not Found
        sendNotFound(client.socket);
        return;
    }

    if (access(clientRequest.path, R_OK) != 0) {
        // Forbidden
        sendForbidden(client.socket);
        return;
    }

    sendData(client.socket, clientRequest.path, statbuf);
}

/**
 * Accept a new connection on a socket
 */
int acceptConnection(Host * server, Host * client) {
    socklen_t size = sizeof(client->address);
    memset(&client->address, 0, sizeof(client->address));

    /* Aguardar conexões */
    client->socket = accept(server->socket, (struct sockaddr *) &client->address, &size);
    if(client->socket < 0) return FAILURE;

    printf("\n[CONEXÃO ACEITA]");
    printf ("\n- Cliente [%s] conectado na porta [%d] do servidor\n", inet_ntoa(client->address.sin_addr), ntohs(client->address.sin_port));

    printf("sock 1 %d\n", server->socket);
    printf("sock 2 %d\n", client->socket);
    return SUCCESS;
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
    int opt = 1;

    if (setsockopt(host->socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int)) < 0) {
        perror("bind:setsockopt failed");
        exit(EXIT_FAILURE);
    }

    if (bind(host->socket, (struct sockaddr *) &host->address, sizeof(host->address)) < 0) {
        perror("bind:bind failed");
        exit(EXIT_FAILURE);
    }
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

/**
 * Create and setup a host
 */
Host createServer(int port) {
    Host host;

    printf(YELLOW "\nInicializando servidor HTTP...\n" RESET);

    /* Create streaming socket */
    createSocket(&host.socket);
	/* Initialize address/port structure */
	memset(&host.address, 0, sizeof(host.address));
    host.address.sin_family = AF_INET;
    host.address.sin_addr.s_addr = htonl(INADDR_ANY);
    host.address.sin_port = htons(port);
    /* Assign a port number to the socket */
    bindSocket(&host);
    /* Make it a "listening socket" */
    listenSocket(host.socket);

    printf(GREEN "\nServidor HTTP inicializado com sucesso...\n" RESET);
    printf("\n****************************************************\n");
    printf(CYAN "\n           Servidor escutando na porta %d\n" RESET, PORT);
    printf("\n****************************************************\n");

    return host;
}

/**
 * Funcao primária da thread
 */
void * runThread(void * hostage) {
    Host server, client;
    Hosts *hosts_aux;
    hosts_aux = (Hosts *) hostage;

    server = hosts_aux->server;
    client = hosts_aux->client;

    printf("sock 11 %d\n", server.socket);
    printf("sock 22 %d\n", client.socket);

    resolve(server, client);
    /* Fecha a conexão com o cliente */
    close(client.socket);
    pthread_exit(NULL);
}

/**
 * Funcao pra criar a thread
 */
void createThread(Host server, Host client, pthread_t thread){
    Hosts hosts;
    hosts.server = server;
    hosts.client = client;
    pthread_create(&thread, NULL, runThread, (void *) &hosts);
}

int main(int argc, char **argv) {
    Host server = createServer(PORT);
    Host clients[MAX_THREAD];

    // Iniciando o vetor de clientes com o valor 0
    int i;
    for (i=0; i < MAX_THREAD; i++)
        clients[i].socket = 0;
    int threadCounter = 0;
    pthread_t thread[MAX_THREAD];

	while(1) {
        // Se o socket do client[threadCounter] estiver com o valor de inicio 0 (o que significa que nao esta sendo usado)
        if (clients[threadCounter].socket == 0) {
            if(acceptConnection(&server, &clients[threadCounter]) == FAILURE) {
                perror("accept");
                continue;
            }
            printf("\n[Contador de Threads: %d]\n", threadCounter);
            // Cria a Thread para o cliente
            createThread(server, clients[threadCounter], thread[threadCounter]);
        }

        // Implementando um vetor circular, se client sock nao estiver zerado (valor == 0))
        if (clients[threadCounter].socket != 0) {
            if (threadCounter > MAX_THREAD )        // Se o indice de vetor threadCounter for maior que a capacidade maxima de threads MAX_THREAD
                threadCounter = 0;                  // Reinicia o indice threadCOD do vetor clients com 0
            else threadCounter++;                   // Senão, incrementa o indice do vetor clients
        }
	}
	// Fecha o socket servidor (deve nunca chegar aqui)
	close(server.socket);
    return EXIT_FAILURE;
}
