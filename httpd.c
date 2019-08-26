#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <ctype.h>

#define BUFFSIZE        8192
#define DEFAULT_MAXCONN 20
#define S_OK            0
#define S_FAIL          1

typedef struct 
{
    int fd;
    int flag;
} connInfo;

int SendResponse(int fd_cliesock, int errCode)
{
    unsigned char buf[BUFFSIZE] = { 0 };

    switch (errCode)
    {
    case 200:
        strcpy(buf, "HTTP/1.1 200 OK\r\n"
                    "Cache Control: private\r\n"
                    "content-type: text/html\r\n"
                    "Server: SimpleServer\r\n\r\n");
        break;
    case 501:
        strcpy(buf, "HTTP/1.1 501 Not Implemented\r\n"
                    "Cache Control: private\r\n"
                    "content-type: text/html\r\n"
                    "Server: SimpleServer\r\n\r\n"
                    "<html><h1>501 Method Not Implemented!</h1><hr>SimpleServer</html>");
        break;
    case 404:
        strcpy(buf, "HTTP/1.1 404 Not Found\r\n"
                    "Cache Control: private\r\n"
                    "content-type: text/html\r\n"
                    "Server: SimpleServer\r\n\r\n"
                    "<html><h1>404 Not Found!</h1><hr>SimpleServer</html>");
        break;
    default:
        break;
    }

    send(fd_cliesock, buf, strlen(buf), 0);

    return S_OK;
}

int procResponse(int fd_cliesock, int flag_log, unsigned char buf[])
{
    unsigned char path[BUFFSIZE] = "./www";
    unsigned char content[BUFFSIZE] =  { 0 };
    struct stat pathinfo;
    size_t len_content;
    FILE *pf = NULL;

    if (!strncmp(buf, "GET", 3))
    {
        sscanf(buf, "GET %s ", path + 5);
        // default index.html
        stat(path, &pathinfo);
        if (S_ISDIR(pathinfo.st_mode))
            strcat(path, "/index.html");
        // open file and send
        pf = fopen(path, "r");
        if (!pf)
            SendResponse(fd_cliesock, 404);
        else
        {
            SendResponse(fd_cliesock, 200);
            while ((len_content = fread(content, sizeof(unsigned char), BUFFSIZE, pf)) > 0)
            {
                send(fd_cliesock, content, len_content, 0);
            }
            fclose(pf);
        }
    }
    else if (!strncmp(buf, "POST", 4))
    {
        sscanf(buf, "POST %s ", path + 5);
        // default index.html
        stat(path, &pathinfo);
        if (S_ISDIR(pathinfo.st_mode))
            strcat(path, "/index.html");
        // open file and send
        pf = fopen(path, "r");
        if (!pf)
            SendResponse(fd_cliesock, 404);
        else
        {
            SendResponse(fd_cliesock, 200);
            while ((len_content = fread(content, sizeof(unsigned char), BUFFSIZE, pf)) > 0)
            {
                send(fd_cliesock, content, len_content, 0);
            }
            fclose(pf);
        }
    }
    else
    {
        SendResponse(fd_cliesock, 501);
    }

    return S_OK;
}
void* procRequest(void *data)
{
    unsigned char buf[BUFFSIZE] = { 0 };
    int fd_cliesock = ((connInfo*)data)->fd;
    int flag_log = ((connInfo*)data)->flag;
    int len_recv;

    if((len_recv = recv(fd_cliesock, buf, BUFFSIZE, 0)) > 0)
    {
        buf[len_recv] = 0;
        if (flag_log)
            printf("log >> new data request!\n%s\n", buf);
        procResponse(fd_cliesock, flag_log, buf);
        close(fd_cliesock);
    }

    return S_OK;
}

int procConn(int fd_servsock, int flag_log)
{
    connInfo tStru;        // record client socket file descriptor & log flag
    struct sockaddr_in sca;     // record addr(client)
    socklen_t len_cfd = sizeof(struct sockaddr);
    int fd_cliesock;     // file descriptor of client socket
    pthread_t tid;
    pthread_attr_t attr_th;

    pthread_attr_init(&attr_th);
    pthread_attr_setdetachstate(&attr_th, PTHREAD_CREATE_DETACHED);
    while (1)
    {
        if ((fd_cliesock = accept(fd_servsock, (struct sockaddr*)&sca, &len_cfd)) < 0)
        {
            perror("log >> fail to connect");
        }
        else
        {
            if (flag_log)
                printf("log >> new client socket!\n");
            tStru.fd = fd_cliesock;
            tStru.flag = flag_log;
            pthread_create(&tid, &attr_th, procRequest, (void*)&tStru);
        }
    }

    return S_OK;
}

int initSocket(int *fd_servsock, int flag_log, int port, int max_conn)
{
    struct sockaddr_in ssa; // record addr(server)
    int _fd;

    if ((_fd = socket(PF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("log >> fail to create socket");
        return S_FAIL;
    }
    else if (flag_log)
        printf("log >> create socket!\n");
    memset(&ssa,0,sizeof(struct sockaddr_in));
    ssa.sin_addr.s_addr = htonl(INADDR_ANY);
    ssa.sin_family = AF_INET;
    ssa.sin_port = htons(port);
    if (bind(_fd, (struct sockaddr*)&ssa,sizeof(struct sockaddr)) < 0)
    {
        perror("log >> fail to bind address");
        return S_FAIL;
    }
    else if (flag_log)
        printf("log >> bind address!\n");
    if (listen(_fd, max_conn) < 0)
    {
        perror("log >> fail to set listen");
        return S_FAIL;
    }
    else if (flag_log)
        printf("log >> set listen!\n");
    printf("Establishing...\n");
    *fd_servsock = _fd;

    return S_OK;
}

int main(int argc, char *argv[])
{
    int flag_log = 0;       // flag of log output
    int start = 0;          // flag of start
    int fd_servsock = 0;    // file descriptor of server socket
    int max_conn = DEFAULT_MAXCONN;      // max connection
    int i_argu;

    if (argc == 1 || (argc == 2 && !strcmp(argv[1], "-h")))
    {
        printf("Put website file under ./www and start the server.\n"
                "start:\t\tstart server\n"
                "-l:\t\tenable log output\n"
                "-c [digit]:\tset max connection\n"
                "-h:\t\tshow help\n");
    }
    else
    {
	
        for (i_argu = 1; i_argu < argc; i_argu++)
        {
            if (!strcmp(argv[i_argu], "-l"))
                flag_log = 1;
            else if (!strcmp(argv[i_argu], "-c"))
            {
                if (i_argu + 1 < argc)
                    max_conn = atoi(argv[i_argu]) ? atoi(argv[i_argu]) : DEFAULT_MAXCONN;
            }
            else if (!strcmp(argv[i_argu], "start"))
                start = 1;
        }
        if (start)
        {
            if (initSocket(&fd_servsock, flag_log, 80, max_conn) == S_FAIL)
                return S_FAIL;
            procConn(fd_servsock, flag_log);
        }
    }
    
    return S_OK;
}