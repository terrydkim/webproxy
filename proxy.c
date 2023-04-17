#include <stdio.h>

#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

int main(int argc, char **argv)
{ // 첫 매개변수 argc는 옵션의 개수, argv는 옵션 문자열의 배열
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  /* Check command line args */
  if (argc != 2)
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  /* Open_listenfd 함수 호출 -> 듣기 식별자 오픈, 인자를 통해 port번호 넘김 */
  listenfd = Open_listenfd(argv[1]);

  /* 무한 서버 루프 실행 */
  while (1)
  {
    clientlen = sizeof(clientaddr); // accept 함수 인자에 넣기 위한 주소 길이 계산

    /* 반복적 연결 요청 접수 */
    // Accept(듣기 식별자, 소켓 주소 구조체 주소, 해당 주소 길이)
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);

    // Getaddrinfo => 호스트 이름: 호스트 주소, 서비스 이름: 포트 번호의 스트링 표시를 소켓 주소 구조체로 변환
    // Getnameinfo => 위의 Getaddrinfo의 반대로, 소켓 주소 구조체 -> 스트링 표시로 변환
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);

    /* 트랜잭션 수행 */
    doit(connfd);
    /* 트랜잭션이 수행된 후, 자신 쪽의 연결 끝(소켓)을 닫음*/
    Close(connfd);
  }
}

void doit(int fd)
{
  int proxyfd;
  char host, port[MAXLINE], path[MAXLINE];
  char buf_client[MAXLINE], buf_server[MAXLINE];
  char method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  rio_t rio_client, rio_server;

  Rio_readinitb(&rio_client, fd);
  Rio_readlineb(&rio_client, buf_client, MAXLINE);
  sscanf(buf_client, "%s %s %s", method, uri, version);
  printf("---------------------------\n");
  printf("Request headers:\n");
  printf("%s", buf_client);

  if (strcasecmp(method, "GET"))
  {
    // GET이 아닌 다른 메소드인 경우 501 Not Implemented 에러를 반환합니다.
    clienterror(fd, method, "501", "Not Implemented",
                "Tiny does not implement this method");
    return;
  }

  proxyfd = Open_clientfd(host, "9501"); // new socket open
  Rio_readinitb(&rio_server, proxyfd);
  Rio_writen(proxyfd, buf_client, strlen(buf_client));

  printf("&&&&&\n");
  printf("aaaa%s", buf_client);
  while (strcmp(buf_client, "\r\n"))
  {
    printf("2======\n");
    Rio_readlineb(&rio_client, buf_client, MAXLINE);
    Rio_writen(proxyfd, buf_client, strlen(buf_client));
  }
  printf("ddd\n");
  // printf(" server\n");
  // read_requesthdrs(&rio_client);
  // Rio_readinitb(&rio_server, proxyfd);
  // sprintf(buf_server, "%s %s %s\r\n", method, path, version);
  // printf("buf : %s\n", buf_server);

  // 안넘어감
  //  printf("==============");
  //  Rio_writen(proxyfd, buf_server, strlen(buf_server));
  //  printf("0000000000000");

  // printf("xxxxxxxxxx");

  Rio_readlineb(&rio_server, buf_server, MAXLINE);
  Rio_writen(fd, buf_server, strlen(buf_server));

  while (strcmp(buf_server, "\r\n"))
  { // proxyfd로부터 데이터를 읽어옴
    printf("Proxy response: %s", buf_server);
    Rio_readlineb(&rio_server, buf_server, MAXLINE);
    Rio_writen(fd, buf_server, strlen(buf_server)); // fd에 buf의 데이터를 전송
  }
  // //content-length 받기 malloc에 할당
  // //코드 맞게 고쳐야 됨
  
  // srcp = malloc(filesize);
  // Rio_readnb(srcfd, srcp, filesize); // srcfd에서 filesize 만큼 srcp에 저장
  // Rio_writen(fd, srcp, filesize);

  // free(srcp);
  Close(proxyfd);
}

void read_requesthdrs(rio_t *rp)
{
  char buf[MAXLINE];

  Rio_readlineb(rp, buf, MAXLINE);
  printf("%s", buf);
  printf("******\n");
  while (strcmp(buf, "\r\n"))
  {
    Rio_readlineb(rp, buf, MAXLINE);
    // Rio_writen(proxyfd, buf_server, strlen(buf_server));
    printf("%s", buf);
    printf("&&&&\n");
  }
  return;
}
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
  char buf[MAXLINE];

  /* Print the HTTP response headers */
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n\r\n");
  Rio_writen(fd, buf, strlen(buf));

  /* Print the HTTP response body */
  sprintf(buf, "<html><title>Tiny Error</title>");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "<body bgcolor="
               "ffffff"
               ">\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "%s: %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "<p>%s: %s\r\n", longmsg, cause);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "<hr><em>The Tiny Web server</em>\r\n");
  Rio_writen(fd, buf, strlen(buf));
}