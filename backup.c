/*
 * Projeto servidor http
 *
 */
/*** Bibliotecas necessÃ¡rias */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>   /* Biblioteca para se utilizar a struct stat para verificaÃ§Ã£o de arquivos*/
#include <fcntl.h>      /* Biblioteca para se utilizar a funÃ§Ã£o open()*/
#include <sys/socket.h> /* Biblioteca contendo as definicoes de sockets*/
#include <sys/wait.h>   /* Biblioteca para utillizar a funÃ§Ã£o waitpid() */
#include <sys/types.h>  /* Biblioteca contendo os tipos de socket */
#include <arpa/inet.h>  /* Biblioteca contendo Funcoes referentes ao inet (Rede) */
#include <unistd.h>     /* Biblioteca contendo varios comandos *NIX */
#include <pthread.h>    /* Biblioteca para utilizacao de THREADS para otimizar o acesso*/

/**
 * Parametros utilizados pelo programa
 */
#define SERVERNAME  "Audemar & Fernando WebServer"    /* Nome do sistema */
#define PORTA       (7777)                /* Porta utilizada pelo server */
#define PROTOCOLO   "HTTP/1.1"                /* Protocolo HTTP (VersÃ£o) */
#define PEND        (1024)                /* Tamanho maximo da fila de conexoes pendentes */
#define MAXBUF      (4096)                /* Tamanho de buffer */
#define RFC1123     "%a, %d %b %Y %H:%M:%S GMT"        /* Formato de hora para cabecalho*/
#define MAX_THREAD  5                    /* Tamanho maximo de threads*/
#define DIR_ROOT    "/home/"            /* Diretorio onde ficam as paginas */

/************************ Estruturas **********************/
typedef enum{
    false,
    true
}Boolean;
/**
 * Estrutura utilizada para guardar dados pertinentes a um host
 */
typedef struct{
    int socket;
    struct sockaddr_in destino;
}Host;

/**
 * Estrutura utilizada para organizar a requesicao de um client, fazendo a divisao do path "diretorio" + protocolo versao
 */
typedef struct{
    char line[MAXBUF];
    char metodo[4];        //Nesse codigo so esta implementado o metodo GET
    char recurso[1000];         //Recurso requisitado pelo cliente . Ex: index.html ; random/image.gif...
    char protocolo[20];         //Versao do Protocolo http
}Request;

/**
 * Estrutura utilizada para guardar os returns da funcao checkRequest
 */
typedef struct{
    char dir[MAXBUF];        // diretorio local do arquivo
    struct stat statBuffer;    // Montar a estrutura para verificaÃ§Ã£o de estado dos arquivos
    int n;            // Abrir o arquivo local como "inteiro"
    int answerCOD;            // Resposta do servidor HTTP. Ex: 200, 404 ...
}CR_returns;

/**
 * Estrutura utilizada para guardar os dados do server e do cliente corrente
 */
typedef struct{
    Host server;
    Host client;
}Hosts;
/********************* Fim das estruturas *******************/



/********************* Funcoes de socket *******************/
int setSocket(int *sockete){
    *sockete = socket(AF_INET, SOCK_STREAM, 0);

    if ( &sockete < 0 )
        return 1;

    return 0;
}

int setBind(Host *name){
    Host aux = *name;

    if ( bind(aux.socket, (struct sockaddr*)&aux.destino, sizeof(aux.destino)) != 0 )
        return 1;

    *name = aux;
    return 0;
}

int setListen(Host name){
    if ( listen(name.socket, PEND) != 0 )
        return 1;

    return 0;
}

int setServer(Host *name, int port){
    Host aux;

    if (setSocket(&aux.socket) == 1)
        return 1;

    //setDestino_server(&aux, port);
    memset(&aux.destino, 0, sizeof(aux.destino));
    aux.destino.sin_family = AF_INET;
    aux.destino.sin_addr.s_addr = htonl(INADDR_ANY);
    aux.destino.sin_port = htons(port);

    if (setBind(&aux) == 1)
        return 3;

    if (setListen(aux) == 1)
        return 4;

    *name = aux;

    return 0;
}


Boolean initAccept(Host *server, Host *client){

    Host serverAux = *server;
    Host clientAux = *client;

    int addrlen = sizeof(clientAux.destino);
    memset(&clientAux.destino, 0, sizeof(clientAux.destino));


    /* Aguardar conexoes*/
    clientAux.socket  = accept(serverAux.socket,(struct sockaddr *)&clientAux.destino, &addrlen);
    if(clientAux.socket < 0)
        return false;

    printf(" [CONNECTION ACCEPTED] \n\n");
    printf (" Cliente [%s] conectado na porta [%d] do servidor \n\n", inet_ntoa(clientAux.destino.sin_addr), ntohs(clientAux.destino.sin_port));

    *server = serverAux;
    *client = clientAux;

    return true;
}
/****************** Fim das funcoes de socket ***************/

/*********************** FunÃ§Ãµes Variadas do Servidor Http**/

/**
 * FunÃ§Ã£o que retorna o tipo de content-type, http://www.jbox.dk/sanos/webserver.htm
 */
char *get_mime_type(char *name){
    char *ext = strrchr(name, '.');
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

/**
 * FunÃ§Ã£o para fazer um cabeÃ§alho padrÃ£o HTTP
 */
void httpHeader(int client_socket,char *mimeType, int tamanho, time_t ultAtu, int request_check, char *check_type) {

    time_t agora;
    char bufHora[128];
    char buffer[100];

    sprintf(buffer, "%s %d %s\r\n", PROTOCOLO, request_check,check_type);
    write(client_socket, buffer, strlen(buffer));

    sprintf(buffer, "Server: %s\r\n", SERVERNAME);
    write(client_socket, buffer, strlen(buffer));

    agora = time(NULL);
    strftime(bufHora, sizeof(bufHora), RFC1123, gmtime(&agora));

    sprintf(buffer, "Date: %s\r\n", bufHora);
    write(client_socket, buffer, strlen(buffer));

    sprintf(buffer, "Content-Type: %s\r\n", mimeType);
    write(client_socket, buffer, strlen(buffer));

    if (tamanho >= 0) {
        sprintf(buffer, "Content-Length: %d\r\n", tamanho);
        write(client_socket, buffer, strlen(buffer));
    }

    if (ultAtu != -1){
        strftime(bufHora, sizeof(bufHora), RFC1123, gmtime(&ultAtu));
        sprintf(buffer, "Last-Modified: %s\r\n", bufHora);
        write(client_socket, buffer, strlen(buffer));
       }

    write(client_socket, "\r\n", 2);
}

/**
 * FunÃ§Ã£o que envia pÃ¡gina de erro para o cliente  - renderizacao html
 */
int returnErro(int client_socket, char *mensagem, int request_check) {
    char buffer[100];

    sprintf(buffer, "<html>\n<head>\n<title>%s Erro %d</title>\n</head>\n\n", SERVERNAME,request_check);
    write(client_socket, buffer, strlen(buffer));

    sprintf(buffer, "<body>\n<h1>%s Erro %d</h1>\n", SERVERNAME,request_check);
    write(client_socket, buffer, strlen(buffer));

    sprintf(buffer, "<p>%s</p>\n</body>\n</html>\n",mensagem);
    write(client_socket, buffer, strlen(buffer));

    return 0;
}

/**
 * FunÃ§Ã£o para mensagens de erro no console
 */
void saidaErro(char msg[100]){
    fprintf(stderr, "%s: %s\r\n", SERVERNAME, msg);
    exit(EXIT_FAILURE);
}


/******************** Fim de funÃ§Ãµes variadas do Servidor Http *******/


/********************* Funcoes de Request do Client ******************/
/**
 * FunÃ§Ã£o que verifica a Request solicitada pelo browser
 */
CR_returns checkRequest(Host client, Request client_request) {

    CR_returns returns;
    char pathbuf[MAXBUF]; //Guardar informaÃ§Ãµes sobre o caminho do recurso
    int len; //Para auxliar na contagem de tamanho do "caminho"

    strcpy(returns.dir,DIR_ROOT); // Copiar para a variÃ¡vel dir o caminho do DIR_ROOT

    //Se existir o recurso, concatena com a variavel dir, se nao complementa com a pagina inicial /index.html
    if(client_request.recurso)
        strcat(returns.dir,client_request.recurso);
    else strcat(returns.dir,"/index.html");

    printf("\n [PAGE DIR] %s\n", returns.dir);
    printf("\n ______________________________________________________________________________\n\n");

    // Se o tipo de mÃ©todo nao existir returns com check == 501
    if(strcmp(client_request.metodo, "GET") != 0)
        {returns.answerCOD = 501; return returns;}

    // Se o arquivo nao existe returns com check == 404
    if(stat(returns.dir, &returns.statBuffer) == -1)
        {returns.answerCOD = 404; return returns;}

    // Se o arquivo for encontrado e for um diretorio
    if(S_ISDIR(returns.statBuffer.st_mode)) { //Inicio if 1
        len = strlen(returns.dir);

        // Se nÃ£o colocar a barra no final da url, no caso de ser um diretÃ³rio
        if (len == 0 || returns.dir[len - 1] != '/'){ //Inicio if 2

            // Adicionar a "/index.html" barra no final da URL(pelo menos dentro do servidor)
            snprintf(pathbuf, sizeof(pathbuf), "%s/", returns.dir);
            strcat(returns.dir,"/index.html");

            // Se existir o index.html dentro do diretorio, entÃ£o returns com check == 200, se nao existir check == 404
            if(stat(returns.dir, &returns.statBuffer) == -1)
                {returns.answerCOD = 404; return returns;}
            else
                {returns.answerCOD = 200; return returns;}
        }
        else{
            // Se nÃ£o tiver colocar "/" depois do nome do diretÃ³rio entÃ£o colocarÃ¡ apenas index.html
            snprintf(pathbuf, sizeof(pathbuf), "%sindex.html", returns.dir);
            strcat(returns.dir,"index.html");

            // Se existir o index.html dentro do diretorio, entÃ£o returns com answerCOD == 200, se nao existir answerCOD == 404
            if(stat(returns.dir, &returns.statBuffer) >= 0)
                {returns.answerCOD = 200; return returns;}
            else
                {returns.answerCOD = 404; return returns;}
        } //Fim if 2
    }
    // Se for um arquivo que nao seja um diretorio returns com answerCOD == 200, com o endereÃ§o da url completo (statBuffer)
    else
        {returns.answerCOD = 200; return returns;} //Fim if 1
}

/**
 *FunÃ§Ã£o que ler/organiza a requisiÃ§Ã£o do browser
 */
Request readRequest(Host client, FILE *f) {

    Request client_request;
    char line[MAXBUF];
    char *metodo = malloc(4);
    char *recurso = malloc(1000);
    char *protocolo = malloc(20);

    // Colocar os dados do socket(file) na variÃ¡vel "line"
    fgets(line,MAXBUF,f);
    strcpy(client_request.line, line);

    // destrinchando a variavel line. http://www.br-c.org/doku.php?id=strtok
    metodo = strtok(line, " ");
    recurso = strtok(NULL, " ");
    protocolo = strtok(NULL, "\r");

    strcpy(client_request.metodo, metodo);
    strcpy(client_request.recurso,recurso);
    strcpy(client_request.protocolo, protocolo);

    return client_request;
}

/**
 * FunÃ§Ã£o para enviar o arquivo
 */
void sendFile(int client_socket, CR_returns returns){
    char dados;
    int i;

    FILE *arq = fopen(returns.dir, "r");

    // Ver se o sistema tem acesso ao arquivo
    if (!arq) {
        returns.answerCOD = 403;
        httpHeader(client_socket,"text/html",-1,-1, returns.answerCOD, "Forbidden");
        returnErro(client_socket,"Acesso Negado", returns.answerCOD);
    }
    else {
        int tamanho = S_ISREG(returns.statBuffer.st_mode) ? returns.statBuffer.st_size : -1;

        httpHeader(client_socket,get_mime_type(returns.dir),tamanho ,returns.statBuffer.st_mtime, returns.answerCOD, "OK");

        //Enviando
        while ((i = read(returns.n, &dados,1)))
            write(client_socket, &dados, 1);

    }
}

/**
 * FunÃ§Ã£o que envia resposta a requisiÃ§Ã£o do browser
 */
void sendRequest(Host client, CR_returns returns){
    if (returns.answerCOD == 501) // MÃ©todo nÃ£o suportado pelo servidor
            returnErro(client.socket,"Metodo nao suportado", returns.answerCOD);

    if (returns.answerCOD == 404) // Arquivo nÃ£o encontrado
            returnErro(client.socket,"Pagina nao encontrada", returns.answerCOD);

    if (returns.answerCOD == 200){
            returns.n = open(returns.dir, O_RDONLY);
            sendFile(client.socket,returns);
    }
}

/****************** Fim das funcoes de Request *************/

/****************** Funcoes para o Thread *****************/
/**
 * Funcao Secundaria do thread
 */
void function(Host server, Host client){
    printf(" [CONNECTION ACCEPTED] \n\n");
    printf (" Cliente [%s] conectado na porta [%d] do servidor \n\n", inet_ntoa(client.destino.sin_addr), ntohs(client.destino.sin_port));

    /* Jogar os dados do socket(int) em um arquivo para pegar a requisiÃ§Ã£o(line)*/
    FILE *f = fdopen(client.socket,"r+");

    /* Ler a Request do client*/
    Request client_request = readRequest(client, f);

    printf("\n [Client_Request.metodo]    %s \n", client_request.metodo);
        printf("\n [Client_request.recurso]   %s \n", client_request.recurso);
        printf("\n [Client_request.protocolo] %s \n", client_request.protocolo);

    /* checkar a Request do client */
    CR_returns returns =  checkRequest(client, client_request);

    /* enviar resposta da Request ao client */
    sendRequest(client, returns);

    // Fechar arquivo
    fclose(f);

    // Fechar socket
    close(client.socket);
}

/**
 * Funcao Primaria do thread
 * http://pt.wikipedia.org/wiki/Thread_(ci%C3%AAncia_da_computa%C3%A7%C3%A3o)
 * https://computing.llnl.gov/tutorials/pthreads/
 */
void *thread_function (void *hostage){
    Host server, client;
    Hosts *hosts_aux;

    hosts_aux = (Hosts *)hostage;

    server = hosts_aux->server;
    client = hosts_aux->client;

    function(server, client);

    pthread_exit(NULL);
}

/**
 * Funcao pra Ligar o thread
 */
void turnThread_on(Host server, Host client, pthread_t thread ){
    Hosts hosts_;

    hosts_.server = server;
    hosts_.client = client;

    pthread_create(&thread, NULL, thread_function, (void *) &hosts_);
}

/*********************** Funcao principal ******************/
int main() {

    // Limpar a tela
    system("clear");

    printf("\n Inicializando servidor %s...",SERVERNAME);

    Host server;
    if (setServer(&server, PORTA) > 0)
            saidaErro(" Nao foi possivel criar o socket do servidor.");

    printf("\n [Inicializado com sucesso]\n\n");
    printf("\n ***********************************************************");
    printf("\n Eu sou o servidor http em C. Estou escutando a porta [%d]", PORTA);
    printf("\n ***********************************************************\n\n");

    Host client[MAX_THREAD];

    //Iniciando o vetor de clientes com o valor 0
    int i;
    for (i=0; i<MAX_THREAD;i++)
        client[i].socket = 0;

    int threadCOD = 0;
    pthread_t thread[MAX_THREAD];

    while(1) {
        //Se o socket do client[threadCOD] estiver com o valor de inicio 0 (o que significa que nao esta sendo usado)
        if (client[threadCOD].socket == 0){
            //Se a funcao de sokcet accept for false entao retorna erro
            if (initAccept(&server,&client[threadCOD]) == false)
                saidaErro("Nao foi possivel efetuar acao de accept().\n");

            printf("\n[THREAD_COD] %d \n", threadCOD);
            turnThread_on(server, client[threadCOD], thread[threadCOD]);
        }

        //Implementando um vetor circular, se client sock nao estiver zerado (valor == 0))
        if (client[threadCOD].socket != 0){
            if (threadCOD > MAX_THREAD )  //Se o indice de vetor threadCOD for maior que a capacidade maxima de threads MAX_THREAD
                threadCOD = 0; // reinicia o indice threadCOD do vetor client com 0
            else threadCOD++; //se nao incrementa o indice do vetor client "threadCOD" +1
        }

    }//fim do loop
}
