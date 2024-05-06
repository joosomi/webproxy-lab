#include "csapp.h"

void echo(int connfd);

int main(int argc, char **argv) {
    int listenfd, connfd; //서버의 리스닝 소켓, 연결 소켓 파일 디스크립터

    socklen_t clientlen; //클라이언트 주소의 크기

    struct sockaddr_storage clientaddr; //클라이언트 주소 정보를 저장할 구조체
    char client_hostname[MAXLINE], client_port[MAXLINE]; //클라이언트 호스트 이름, 포트 번호 저장할 배열

    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
    }

    //리스닝 소켓을 열고, 저장된 포트에서 연결 기다림
    listenfd = Open_listenfd(argv[1]);

    while (1) {//무한 루프를 통해 연속적으로 클라이언트의 연결을 받아들임
        clientlen = sizeof(struct sockaddr_storage); 
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); //클라이언트로부터의 연결 요청 수락

        //클라이언트 주소 정보를 기반으로 호스트 이름과 포트 번호 알아냄
        Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
        //연결된 클라이언트 정보 출력
        printf("Connected to (%s, %s )\n", client_hostname, client_port);
        echo(connfd); //에코 함수를 호출해서 클라리언트와 데이터 송수신 수행
        Close(connfd); //데이터 송수신이 끝난 후 연결 소켓 닫음
    }    
    exit(0);
}