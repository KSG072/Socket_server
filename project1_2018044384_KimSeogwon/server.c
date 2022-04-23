#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

#define BUFF_SIZE 1024
#define HEADER_FORM "HTTP/1.1 %d %s\nContent-Length: %ld\nContent-Type: %s\n\n"
#define NOT_FOUND "<h1> 404 Not Found </1h>\n"
#define SERV_ERROR "<h1> 500 Internet Server Error </1h>\n"

void http_handler(int*);                    //들어온 http 요청을 받고 그에 맞는 응답을 보내는 함수
void type_handler(char*, char*);            //요청한 파일의 확장자명을 확인하고 그에 맞는 Content-Type을 정하는 함수
void res_error_404(int*);
void res_error_500(int*);
void header_maker(char*, int, long, char*); //파라미터의 값을 기반으로 response header를 만드는 함수

int server, client;

//control + C를 눌러 종료했을 경우 해당함수가 처리해줌
static void clean_exit(int signal){
    printf("\n=================ending server program...=================\n");
    if(close(client) == 0){
        printf("client socket closes successfully.\n");
    }else {printf("fail to closing client socket.\n");}
    if(close(server) == 0){
        printf("server socket closes succussfully.\n");
    }else {printf("fail to closing server socket.\n");exit(1);}
    printf("=============success to end server program...=============\n");
    exit(0);
}

int main(int argc, char** argv){
    int addr_len;
    int status;
    struct sockaddr_in server_addr, client_addr;
    pid_t pid;

    if(argc != 2){
        fprintf(stderr, "unavailable port number");
        exit(1);
    }
    //clean_exit
    signal(SIGTERM, clean_exit);
    signal(SIGINT, clean_exit);

    server = socket(PF_INET, SOCK_STREAM, 0);
    if(server == -1){
        fprintf(stderr, "server socket error");
        exit(1);
    }

    //서버의 sockaddr초기화 및 설정
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(atoi(argv[1]));

    if(bind(server, (struct sockaddr*) &server_addr, sizeof(server_addr)) == -1){
        fprintf(stderr, "bind error");
        exit(1);
    }
    if(listen(server, 10) == -1){
        fprintf(stderr, "listen error");
        exit(1);
    }
    printf("server open successfully.\n");
    while(1){
        addr_len = sizeof(client_addr);
        printf("accepting request...\n");
        client = accept(server, (struct sockaddr*) &client_addr, &addr_len);
        if(client < 0){
            fprintf(stderr, "error eccept");
            continue;
        }
        else{
            pid = fork();
            if(pid == 0){
                http_handler(&client);
                exit(0);
            }
            else if(pid > 0){
                waitpid(pid, &status, 0);
            }
            else{
                fprintf(stderr, "fork error");
                continue;
            }
        }
    }
    return 0;
}

void http_handler(int* client){
    char header[BUFF_SIZE];
    char request_msg[BUFF_SIZE];
    int file_disc;
    struct stat st;
    //http request 메세지 read
    if(read(*client, request_msg, BUFF_SIZE) < 0){
        fprintf(stderr,"read error");
        res_error_500(client);
        exit(1);
    }
    //client가 요청한 파일 파싱
    char* url = strtok(request_msg, " ");
    url = strtok(NULL, " ");
    char* path = getenv("PWD");
    strcat(path,"/rsc");
    //요청한 파일이 없으면 기본 index.html보내기
    if(strlen(url) == 1){
        strcpy(url, "/index.html");
    }
    strcat(path, url);
    printf("path : %s\n", path);

    //파일유무 확인
    if(stat(path, &st) < 0){
        printf("NO HIT FILE\n");
        res_error_404(client);
        exit(1);
    }
    else{
        printf("HIT FILE\n");
    }

    file_disc = open(path, O_RDONLY);
    if(file_disc == -1){
        printf("open error");
        res_error_500(client);
        exit(1);
    }
    else if(file_disc > 0 ){
        printf("open file\n");

        int filesize = st.st_size;
        char filetype[40];
        type_handler(filetype, path);
        header_maker(header, 200, filesize, filetype); // header버퍼에 해당 내용을 기반으로 하여 res 헤더작성
        write(*client, header, strlen(header));
        int file;
        while((file = read(file_disc, request_msg, BUFF_SIZE)) > 0) {
            write(*client, request_msg, file);
        }
        close(file_disc);
        printf("================send success================\n");
    }
}

void type_handler(char* filetype, char* path){
    char* ext = strrchr(path, '.');
    if(strcmp(ext,".html") == 0){
        strcpy(filetype, "text/html");
    }else if(strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0){
        strcpy(filetype, "image/jpeg");
    }else if(strcmp(ext, ".GIF") == 0 || strcmp(ext, ".gif") == 0){
        strcpy(filetype, "image/GIF");
    }else if(strcmp(ext, ".mp3") == 0){
        strcpy(filetype, "audio/mpeg");
    }else if(strcmp(ext, ".pdf") == 0){
        strcpy(filetype, "application/pdf");
    }else{
        strcpy(filetype, "text/plain");
    }
    printf("type : %s\n", filetype);
}

void header_maker(char* header, int status, long filesize, char* filetype){
    char status_text[30];
    if(status == 200) strcpy(status_text, "OK");
    else if(status == 404) strcpy(status_text, "Not Found");
    else if(status == 500) strcpy(status_text, "Internet Server Error");

    sprintf(header, HEADER_FORM, status, status_text, filesize, filetype);
}

void res_error_404(int* client){
    char header[BUFF_SIZE];
    header_maker(header, 500, sizeof(NOT_FOUND), "text/html");
    write(*client, header, strlen(header));
    write(*client, NOT_FOUND, sizeof(NOT_FOUND));
}
void res_error_500(int* client){
    char header[BUFF_SIZE];
    header_maker(header, 500, sizeof(SERV_ERROR), "text/html");
    write(*client, header, strlen(header));
    write(*client, SERV_ERROR, sizeof(SERV_ERROR));
}