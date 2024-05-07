/*
 * adder.c - a minimal CGI program that adds two numbers together
 */
/* $begin adder */
#include "csapp.h"

int main(void) {
  char *buf, *p;
  // char arg1[MAXLINE], arg2[MAXLINE], 
  char content[MAXLINE];
  int n1 = 0, n2 = 0;

  /* Extract the two argumetns*/
  //QUERY_STRING 환경 변수에서 두 인수 추출 -> 값을 buf에 저장
  if ((buf = getenv("QUERY_STRING")) != NULL) {
    p = strchr(buf, '&'); // buf에서 & 문자의 위치를 찾아 p에 저장
    *p = '\0'; //buf를 두 부분으로 나누기 위해 &위치에 NULL 삽입

    // strcpy(arg1, buf);
    // strcpy(arg2, p+1);
    // n1 = atoi(arg1); //atoi(): 문자 string을 정수로 변환 
    // n2 = atoi(arg2);

    //http://localhost:7700/cgi-bin/adder?n1=2&n2=3
    //sscanf() 사용해서 n1, n2 값 추출 
    //buf 문자열에서 n1=로 시작하는 부분 찾고, 그 다음의 숫자를 n1 변수에 저장
    sscanf(buf, "n1=%d", &n1); // buf에서 n1값을 읽어 저장
    //p+1이 가리키는 문자열(& 문자 다음부터 시작하는 문자열)에서 n2=로 시작하는 부분 찾고 그 다음에 오는 숫자를 n2 변수에 저장
    sscanf(p+1, "n2=%d", &n2);
  }

  /*Make the response body*/
  sprintf(content, "QUERY_STRING=%s", buf); 
  sprintf(content, "Welcome to add.com: ");
  sprintf(content, "%sTHE Internet addition portal.\r\n<p>", content);
  sprintf(content, "%sThe answer is: %d + %d = %d\r\n<p>", content, n1, n2, n1 + n2);
  sprintf(content, "%sThanks for visiting!\r\n", content);

  /*Generate the HTTP response*/
  printf("Connedtion: close\r\n");
  printf("Content-length: %d\r\n", (int)strlen(content));
  printf("Content-type: text/html\r\n\r\n");
  printf("%s", content);
  fflush(stdout);

  exit(0);
}
/* $end adder */
