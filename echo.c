#include "csapp.h"


//클라이언트로부터 데이터를 받아 동일한 
void echo(int connfd) {
    size_t n;
    char buf[MAXLINE];
    rio_t rio;

    Rio_readinitb(&rio, connfd);

    //클라이언트로부터 한줄 씩 읽기 - 반환값이 0이면 클라이언트가 연결을 닫았음을 의미
    //loop는 클라이언트 연결이 닫힐 때까지 계속
    while((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) { 
        printf("server received %d bytes\n", (int)n); //서버가 받은 데이터의 바이트 수 출력
        Rio_writen(connfd, buf, n); //받은 데이터를 그대로 다시 보내기
    }
}