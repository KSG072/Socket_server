#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <netdb.h>
#include <fcntl.h>
#define BUFFER_SIZE 65536
#define BUF_SIZE 1024
#define HEADER_FMT "HTTP/1.1 %d %s\nContent-Length: %ld\nContent-Type: %s\n\n"
#define NOT_FOUND_CONTENT "<h1>404 Not Found</h1>\n"
#define SERVER_ERROR_CONTENT "<h1>500 Internal Server Error</h1>\n"


void http_handler(int asock);
void find_mime(char *type, char *url);
void handle_404(int asock);
void handle_500(int asock);
void fill_header(char *header, int status, long len, char *type);

    // void clean_exit(int sigal);//cntl+C를 받아 종료됐을 경우 일어나는 함수

    int main(int argc, char **argv)
{
    int server_sock;
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    int client_sock;
    char message[BUFFER_SIZE];
    pid_t pid;

    //포트번호를 주지 않았을 경우
    if(argc != 2){
        fprintf(stderr, "check if you execute correctly.");
        exit(1);
    }

    //강제종료 처리
    // signal(SIGINT, clean_exit);
    // signal(SIGTERM, clean_exit);

    //서버소켓생성
    server_sock = socket(PF_INET, SOCK_STREAM, 0);
    if(server_sock == -1){
        fprintf(stderr, "fail to make server socket");
        exit(1);
    }

    //server addr초기화
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY); //현재 연결된 ip주소에서 서버열기
    server_addr.sin_port = htons(atoi(argv[1])); //입력으로 준 번호로 포트생성
    
    //바인드 & 리슨
    if(bind(server_sock, (struct sockaddr*) &server_addr, sizeof(server_addr)) == -1){
        fprintf(stderr, "fail to bind");
        exit(1);
    }
    if(listen(server_sock, 6) == -1){
        fprintf(stderr, "fail to listen");
        exit(1);
    }

    while (1){
        int addr_len = sizeof(client_addr);
        client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &addr_len);
        if (client_sock < 0){
            perror("[ERR] failed to accept.\n");
            continue;
        }
        pid = fork();
        if (pid == 0){
            close(server_sock);
            http_handler(client_sock);
            close(server_sock);
            exit(0);
        }
        if (pid != 0){
            close(client_sock);
        }
        if (pid < 0){
            perror("[ERR] failed to fork.\n");
        }
    }
    return 0;
}

void http_handler(int asock)
{
    char header[BUF_SIZE];
    char buf[BUF_SIZE];
    if (read(asock, buf, BUF_SIZE) < 0)
    {
        perror("[ERR] Failed to read request.\n");
        handle_500(asock);
        return;
    }
    char *method = strtok(buf, " ");
    char *url = strtok(NULL, " ");
    if (method == NULL || url == NULL)
    {
        perror("[ERR] Failed to identify method, URI.\n");
        handle_500(asock);
        return;
    }
    printf("[INFO] Handling Request: method=%s, URI=%s\n", method, url);
    char safe_url[BUF_SIZE];
    char *local_url = getenv("PWD");
    strcat(local_url, "/resources");
    struct stat st;
    strcpy(safe_url, url);
    if (!strcmp(safe_url, "/"))
        strcpy(safe_url, "/index.html");
    strcat(local_url, safe_url);

    printf("local_url : %s\n", local_url);
    printf("safe_url : %s\n", safe_url);
    if (stat(local_url, &st) < 0)
    {
        perror("[WARN] No file found matching URI.\n");
        handle_404(asock);
        return;
    }
    int fd = open(local_url, O_RDONLY);
    if (fd < 0)
    {
        perror("[ERR] Failed to open file.\n");
        handle_500(asock);
        return;
    }
    int ct_len = st.st_size;
    char ct_type[40];
    find_mime(ct_type, local_url);
    fill_header(header, 200, ct_len, ct_type);
    write(asock, header, strlen(header));
    int cnt;
    while ((cnt = read(fd, buf, BUF_SIZE)) > 0)
        write(asock, buf, cnt);
}

void handle_404(int asock)
{
    char header[BUF_SIZE];
    fill_header(header, 404, sizeof(NOT_FOUND_CONTENT), "text/html");
    write(asock, header, strlen(header));
    write(asock, NOT_FOUND_CONTENT, sizeof(NOT_FOUND_CONTENT));
}
void handle_500(int asock)
{
    char header[BUF_SIZE];
    fill_header(header, 500, sizeof(SERVER_ERROR_CONTENT), "text/html");
    write(asock, header, strlen(header));
    write(asock, SERVER_ERROR_CONTENT, sizeof(SERVER_ERROR_CONTENT));
}

void fill_header(char *header, int status, long len, char *type)
{
    char status_text[40];
    switch (status)
    {
    case 200:
        strcpy(status_text, "OK");
        break;
    case 404:
        strcpy(status_text, "Not Found");
        break;
    case 500:
    default:
        strcpy(status_text, "Internal Server Error");
        break;
    }
    sprintf(header, HEADER_FMT, status, status_text, len, type);
}

void find_mime(char *type, char *url){
    char* ext = strrchr(url, '.');
    if (!strcmp(ext, ".html")){
        strcpy(type, "text/html");
    }
    else if(!strcmp(ext, ".jpg") || !strcmp(ext, ".jpeg")){
        strcpy(type, "image/jpeg");
    }
    else if(!strcmp(ext, ".png")){
        strcpy(type, "image/png");
    }
    else if(!strcmp(ext, ".GIF")){
        strcpy(type, "image/GIF");
    }
    else if(!strcmp(ext, ".mp4")){
        strcpy(type, "video/mp4");
    }
    else if(!strcmp(ext, ".mp3")){
        strcpy(type, "audio/mpeg");
    }
    else if(!strcmp(ext, ".pdf")){
        strcpy(type, "application/pdf");
    }
    else
        strcpy(type, "text/plain");
}