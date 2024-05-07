/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);

//서버의 기능들은 모두 int main() 함수에 구현되어 있음.
//argc => 커맨드 라인 인자의 개수, **argv => 커맨드 라인 인자들을 가리키는 포인터 배열
int main(int argc, char **argv) {
  int listenfd, connfd; 
  //listenfd : client 연결 요청을 기다리는데 사용되는 소켓의 파일 디스크립터, connfd: 통신을 위한 소켓의 파일 디스크립터
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen; //클라이언트 주소 구조체 크기 저장
  struct sockaddr_storage clientaddr; //클라이언트 주소 정보

  /* Check command line args */
  //.tiny 8000 처럼 argc 가 2개 입력되지 않았다면, 포트 번호가 전달되지 않은 것 -> 프로그램 사용 법 출력하고 프로그램 Exit
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]); //argv[1] 포트를 열어서 들어오는 연결 요청을 기다리는 리스닝 소켓 생성                    
  //무한 반복하여 클라이언트의 연결 요청 처리
  while (1) {
    clientlen = sizeof(clientaddr); //클라이언트 주소 구조체의 크기 설정
    //클라이언트의 연결 요청 수락, 통신을 위한 새로운 소켓 생성(connfd)
    connfd = Accept(listenfd, (SA *)&clientaddr,
                    &clientlen);  // line:netp:tiny:accept
    //클라이언트 주소 정보를 문자열로 변환
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE,0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    //클라이언트 통신 처리
    doit(connfd);   // line:netp:tiny:doit 클라이언트와 통신
    Close(connfd);  // line:netp:tiny:close 통신이 끝나면 연결 종료
  }
}


//클라이언트로부터 요청을 받고 해당 요청이 static or dynamic 콘텐츠 요청하는지 판단한 후
//요청에 맞는 콘텐츠 제공
void doit(int fd) {
  int is_static; //정적 콘텐츠인지 동적 컨텐츠인지 판별하는 변수
  struct stat sbuf; //파일의 상태 정보를 저장할 구조체
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE]; // 파싱된 파일 이름과 CGI 인수를 저장할 배열들
  rio_t rio; // Robust I/O 구조체

  /*Read request line and headers*/
  /*request 라인과 헤더를 읽음*/
  Rio_readinitb(&rio, fd); //RIO 버퍼 초기화
  Rio_readlineb(&rio, buf, MAXLINE);  //request 라인 읽음
  printf("%s", buf);
  sscanf(buf, "%s %s %s", method, uri, version); //request line 파싱 -> 메소드, URI, 버전 추출
  //메소드가 GET이 아닌 경우 -> 클라이언트에게 501 에러
  if (strcasecmp(method, "GET")) {
    clienterror(fd, method, "501", "Not implemented",
        "Tiny does not implement this method");
        return;
  }

  //Request header 읽음
  read_requesthdrs(&rio);

  /*Parse URI from GET request, GET 요청에서 URI 파싱*/
  is_static = parse_uri(uri, filename, cgiargs); //URI 파싱해서 정적/동적 콘텐츠 판별 
  //파일 상태 정보를 가져오는데 실패한 경우 => 클라잉너트에게 404에러
  if (stat(filename, &sbuf) < 0) {
    clienterror(fd, filename, "404", "Not found",
          "Tiny couldn't find this file");
    return;
  }

  /*Serve static content, 정적 콘텐츠 제공*/
  if (is_static) { 

    /*파일이 일반 파일이 아니거나 읽기 권한이 없는 경우 -> 403 에러*/
    //S_ISREG(sbuf.st_mode): 파일 모드가 정규 파일을 가맄키는지 ->false 반환하면 정규 파일이 아님(디렉토리나 링크일 수 있음)
    // S_IRUSR :소유자의 읽기 권한, sbuf.st_mode : 권한 비트 -> 해당 권한이 설정되었는지 검사
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
      clienterror(fd, filename, "403", "Forbidden",
            "Tiny couldn't read the file");
      return;
    }

    serve_static(fd, filename, sbuf.st_size); //정적 콘텐츠 제공
  }
  /*Serve dynamic content 동적 콘텐츠 제공*/
  else { 
    //파일이 일반 파일이 아니거나 소유자가 실행 권한을 갖고 있지 않은 경우 -> 403에러 
    //S_IXUSR: 파일 소유자의 실행 권한
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {
      clienterror(fd, filename, "403", "Forbidden",
            "Tiny couldn't run the CGI program");
      return;
    }
    serve_dynamic(fd, filename, cgiargs); //동적 콘텐츠 제공
  }

}

//클라이언트에게 에러 메시지 전송 -> HTML 형식의 에러 페이지를 구성하여 클라이언트에게 전송
//sprintf() 함수: printf()와 유사하게 동작하지만 출력 결과를 화면에 표시하는 대신 지정된 문자 배열(buffer)에 저장
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg) {
  char buf[MAXLINE], body[MAXBUF];

  /* Build the HTTP response body, HTTP 응답 본문*/
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause); //긴 메시지와 원인
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body); 


  /*Print the HTTP response*/
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf)); //buf에 저장된 HTTP 헤더 정보를 fd를 통해 연결된 클라이언트에게 정확한 길이만큼 전송
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}


//HTTP 요청 헤더를 읽고 출력하는 역할
void read_requesthdrs(rio_t *rp) {
  char buf[MAXLINE];

  Rio_readlineb(rp, buf, MAXLINE); //첫번째 헤더 라인 읽음
  //헤더의 끝(빈줄)까지 loop
  while(strcmp(buf, "\r\n")) {
    Rio_readlineb(rp, buf, MAXLINE); // 다음 헤더 라인 읽고 
    printf("%s", buf); //출력
  }
  return;
}


//URI를 분석하여 정적, 동적 콘텐츠 처리
//URI에 따라 웹 서버가 제공해야 할 콘텐츠가 정적인지 동적인지 결정 -> 그에 따른 적절한 파일 이름과 CGI 인자 설정
int parse_uri(char *uri, char *filename, char *cgiargs) {
  char *ptr;

  /*Static content 정적 콘텐츠 처리*/
  //strstr() : 첫번째 인자(uri)에서 두번쨰 인자("cgi-bin")의 첫 번째 발생을 찾아 그 위치의 포인터 반환
  // 존재하지 않으면 NULL 반환  -> 동적 콘텐츠를 처리하는 CGI 프로글매과 관련이 없는 것 => 정적 콘텐츠 요청하는 것
  if (!strstr(uri, "cgi-bin")) {
    strcpy(cgiargs, ""); // 정적 콘텐츠 요청하는 경우 CGI 인자가 필요없으니 빈 문자열 복사
    strcpy(filename, "."); //현재 디렉토리로 기본 경로 설정
    strcat(filename, uri); //URI를 파일 이름에 추가

    //URI가 /로 끝나면 기본 파일 이름을 home.html로 설정
    //strlen(url)-1 : '\0'를 제외한 문자수 -> 마지막 문자가 /인지 확인 
    //uri가 디렉토리를 가리키는지 체크
    if (uri[strlen(uri)-1] == '/') {
      strcat(filename, "home.html");  //filename 문자열의 끝에 host.html 문자열 붙임 
      //웹 서버가 디렉토리에 대한 요청을 받았을 때 사용자에게 보여줄 기본적인 웹 페이지 설정
      //strcat 함수 -> 두개의 문자열을 연결하는데 사용.
    }
    return 1; 
  }

  /*Dynamic content 동적 콘텐츠 처리*/
  else {
    //index(): 주어진 문자열에서 특정 문자를 받아 그 위치의 포인터 반환
    ptr = index(uri, '?');
    if (ptr) {
      strcpy(cgiargs, ptr+1); //쿼리 스트링을 cgiargs로 복사
      *ptr = '\0'; //? 위치를 널 문자로 대체 ->  URI의 경로 부분만 남기기 위함
    }
    // '?'가 없다면 CGI 인자 비움
    else 
      strcpy(cgiargs, "");

    strcpy(filename, "."); //파일 이름의 기본 경로를 현재 디렉토리로 설정
    strcat(filename, uri); // ? 이전 부분을 파일 이름에 추가 
    return 0;
  }
}

//serve_static: 정적 콘텐츠를 클라이언트에게 제공
//정적 콘텐츠란 서버에 미리 저장되어 있는 (HTML, CSS, 이미지 파일)
//클라이언트의 요청에 따라 변경되지 않고 그대로 전송됨
//파일 이름과 파일 크기를 인자로 받아서 해당 파일을 클라이언트에게 전송
void serve_static(int fd, char *filename, int filesize){
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF];

  /*Send response headers to client*/
  get_filetype(filename, filetype); //파일 이름을 바탕으로 파일의 MIME 타입 결정
  sprintf(buf, "HTTP/1.0 200 OK\r\n"); // HTTP 응답 시작 
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf); //서버 정보
  sprintf(buf, "%sConnection: close\r\n", buf); //연결 닫음
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize); //콘텐츠 길이
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype); //콘텐츠 타입
  Rio_writen(fd, buf, strlen(buf)); //
  printf("Response headers: \n");
  printf("%s", buf);


  /*Send response body to client*/
  srcfd = Open(filename, O_RDONLY, 0); //요청받은 파일을 읽기 전용 모드(O_RDONLY)로 열기 
  
  srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0); //요청한 파일을 가상 메모리에 매핑 
  //-> 파일의 내용을 메모리 주소 공간에 직접 매핑하여 파일 I/O 연산 없이 메모리에 직접 데이터를 읽어올 수 있음
  
  Close(srcfd); //파일 디스크립터 srcfd를 닫음 -> 파일이 메모리에 매핑된 이후에는 파일 디스크립터가 더 이상 필요하지 않음
  //-> 닫지 않으면 치명적인 메모리 누수가 발생할 수 있음

  Rio_writen(fd, srcp, filesize); //fd를 통해 매핑된 파일 내용(srcp가 가리키는 내용)을 클라이언트에 전송
  Munmap(srcp, filesize); //메모리 매핑 해제 -> 파일 내용 전송이 완료되면 더이상 메모리 매핑이 필요하지 않기 때문에 메모리 해제
}

/*
get_filetype - Derive file type from filename
파일 이름 기반으로 파일 유형(MIME 타입) 결정
filename: 파일 유형을 결정할 파일의 이름
filetype: 결정된 파일 유형을 저장할 문자열 주소
*/
void get_filetype(char *filename, char *filetype) {
  //파일 이름에 .html 확장자가 포함되어 있으면 
  // HTML 문서로 간주하고 MIME Type을 text/html로 설정
  if (strstr(filename, ".html")) {
    strcpy(filetype, "text/html");
  }
  //파일 이름에 .gif 확장자가 포함되어 있으면
  //MIME Type - GIF 이미지로 간주
  else if (strstr(filename, ".gif")) {
    strcpy(filetype, "image/gif");
  }
  //파일 이름에 .png 확장자가 포함되어 있으면
  //PNG 이미지로 간주 -> MIME type : "iamge/png"
  else if (strstr(filename, ".png")) {
    strcpy(filetype, "image/png");
  }
  //파일 이름에 .jpg 확장자가 포함되어 있으면
  //JPEG 이미지로 간주 -> MIME type: "image/jpeg"
  else if (strstr(filename, ".jpg")) {
    strcpy(filetype, "image/jpeg");
  }
  //기볹거으로 "text/plain"으로 설정ㄴ
  else 
    strcpy(filetype, "text/plain");
}

// serve_dynamic
//동적 내용을 처리하기 위해 웹 서버에서 사용되는 함수
//CGI 프로그램을 실행하고 그 출력을 클라이언트에게 직접 전송
void serve_dynamic(int fd, char *filename, char *cgiargs) {
  char buf[MAXLINE], *emptylist[] = {NULL};

  /*Return first part of HTTP response*/
  //HTTP 응답의 첫 부분을 클라이언트에게 반환 -> 200: 요청이 성공적으로 처리되었음
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));

  //서버 정보를 클라이언트에게 보냄
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));

  /*Child*/
  //자식 프로세스에서 실행되는 코드
  if (Fork() == 0) {
    /*Real server would set all CGI vars here*/ //CGI 프로그램에 전달될 QUERY_STRING 환경 변수 설정
    setenv("QUERY_STRING", cgiargs, 1);
    Dup2(fd, STDOUT_FILENO); /*Redirect stdout to client, 자식 프로세스의 표준 출력(STDOUT)을 클라이언트 연결의 파일 디스크립터로 redirect*/
    Execve(filename, emptylist, environ); /*Run CGI program* CGI 프로그램 실행 */
    //filename 변수에는 실행할 CGI 프로그램의 경로가 저장되어 있음. 
    //emptylist는 CGI 프로그램으로 전달될 인자 목록, 여기서는 인자 없이 실행됨
  }
  Wait(NULL); /*Parent waits for and reaps child 부모는 자식 프로세스가 종료될 때까지 기다림, 자식 프로세스가 종료되면 시스템 자원 회수*/
}

//Fork() : 현재 실행중인 프로세스(부모 프로세스)의 정확한 복사본 생성 - 자식 프로세스 생성
  // 자식 프로세스는 부모 프로세스와 메모리 공간을 공유하지 않음. 자신만의 독립적인 메모리 공간을 가짐
  // 부모 프로세스에게는 자식 프로세스의 PID가 반환, 자식 프로세스에서는 0이 반환.

  // Fork() == 0이 참이면 현재 자식 프로세스의 컨텍스트에서 실행되고 있음을 의미
  // 부모 프로세스에서는 이 조건이 거짓이 됨
  
  /*동적 콘텐츠를 제공하기 위해 CGI 프로그램을 별도의 프로세스로 실행하기 위해 
  자식 프로세스는 독립적인 환경에서 CGI 프로그램 실행 -> 그 출력을 클라이언트에게 전송
  웹 서버의 메인 프로세스는 다른 요청을 처리할 수 있음 
  부모 프로세스는 자식 */

//Dup2(): 열린 File descriptor를 복제하는 함수