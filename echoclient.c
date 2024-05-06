#include "csapp.h"

//0번째 인자 : 실행 파일, 1번쨰 인자: host name, 2번째 : 포트 번호
int main(int argc, char **argv) {
    int clientfd; //클라이언트 소켓 파일 디스크립터
    char *host, *port, buf[MAXLINE]; //호스트, 포트, 데이터 버퍼
    rio_t rio; //Robust I/O 구조체

    if (argc != 3) {
        fprintf(stderr, "usage: %s <host> <port>\n", argv[0]);
        exit(0);
    } 

    host = argv[1]; //첫 번쨰 인자 -> 호스트 이름
    port = argv[2]; //두 번째 인자 -> 포트 번호

    //주어진 호스트와 포트에 대한 클라이언트 소켓을 연다
    clientfd = Open_clientfd(host, port);

    //클라이언트 소켓 파일 식별자와 읽기 버퍼 rio 연결
    Rio_readinitb(&rio, clientfd);

    //표준 입력에서 텍스트 줄을 반복적으로 읽음
    //표준 입력 Stdin에서 MAXLINE만큼 바이트를 가져와 buf에 저장
    //fgets가 EOF 표준 입력을 만나면 종료 (사용자가 Ctrl + D를 눌렀거나 파일로 텍스트 줄을 모두 소진)
    while (Fgets(buf, MAXLINE, stdin) != NULL) {
        Rio_writen(clientfd, buf, strlen(buf)); //사용자 입력을 서버에 전송
        Rio_readlineb(&rio, buf, MAXLINE); //서버로부터 응답을 받음
        Fputs(buf, stdout); //받은 응답을 표준 출력으로 출력
    }
    Close(clientfd); //클라이언트 소켓을 닫음
    exit(0); //클라이언트 종료
}
 