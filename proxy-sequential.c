#include <stdio.h>

#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

void parse_uri(char *uri, char *host, char *port, char *path);
void doit(int fd);

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
  char host[MAXLINE], port[MAXLINE], path[MAXLINE];
  char buf_client[MAXLINE], buf_server[MAXLINE];
  char method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  rio_t rio_client, rio_server;

  // 클라에서 요청 수신
  Rio_readinitb(&rio_client, fd);
  Rio_readlineb(&rio_client, buf_client, MAXLINE);
  sscanf(buf_client, "%s %s %s", method, uri, version);
  printf("----client----");
  printf("Request headers:\n");
  printf("%s", buf_client);
  if (strcasecmp(method, "GET"))
  {
    // GET이 아닌 다른 메소드인 경우 501 Not Implemented 에러를 반환합니다.
    clienterror(fd, method, "501", "Not Implemented",
                "Tiny does not implement this method");
    return;
  }

  // 클라 요청에서 목적 서버의 호스트 및 포트 추출
  parse_uri(uri, host, port, &path);

  // 추출한 호스트, 포트 정보로 목적 서버로 요청날리기
  proxyfd = Open_clientfd(host, port); // new socket open
  sprintf(buf_server, "%s %s %s \r\n", method, path, version);
  printf("---to server---");
  printf("%s\n", buf_server);

  Rio_readinitb(&rio_server, proxyfd);
  // 요청에서 받은 헤더를 바꿔서 서버로 보내기
  modify_http_header(buf_server, host, port, path, &rio_client);
  Rio_writen(proxyfd, buf_server, strlen(buf_server));

  size_t n;
  while ((n = Rio_readlineb(&rio_server, buf_server, MAXLINE)) != 0)
  {
    printf("프록시에서 %d byte 받았음. 클라한테 보낼거임", n);
    Rio_writen(fd, buf_server, n);
  }
  Close(proxyfd);
}

// 목적지 서버에 보낼 HTTP 요청 헤더로 수정하기
void modify_http_header(char *http_header, char *hostname, int port, char *path, rio_t *server_rio)
{
  char buf[MAXLINE], other_hdr[MAXLINE], host_hdr[MAXLINE];

  sprintf(http_header, "GET %s HTTP/1.0\r\n", path);

  while (Rio_readlineb(server_rio, buf, MAXLINE) > 0) // 데이터를 읽어오면 실제 읽어온 데이터 byte만큼 return
  {
    if (strcmp(buf, "\r\n") == 0)
      break;

    if (!strncasecmp(buf, "Host", strlen("Host"))) // buf에서 host있는지 확인
    {
      strcpy(host_hdr, buf); // 있으면 host_hdr에 저장
      continue;
    }
    // connection, proxy-connection, user-agent가 하나라도 맞으면 0 을 반환하므로 if문이 거짓이 됨. 따라서 아닌것들만 other_hdr에 저장
    if (strncasecmp(buf, "Connection", strlen("Connection")) && strncasecmp(buf, "Proxy-Connection", strlen("Proxy-Connection")) && strncasecmp(buf, "User-Agent", strlen("User-Agent")))
    {
      strcat(other_hdr, buf);
    }
  }
  // host_hdr이 비어있으면 저장
  if (strlen(host_hdr) == 0)
  {
    sprintf(host_hdr, "Host: %s:%d\r\n", hostname, port);
  }
  // 헤더 만들기
  sprintf(http_header, "%s%s%s%s%s%s%s", http_header, host_hdr, "Connection: close\r\n", "Proxy-Connection: close\r\n", user_agent_hdr, other_hdr, "\r\n");
  return;
}

void parse_uri(char *uri, char *host, char *port, char *path)
{
  // http://hostname:port/path 형태
  char *ptr = strstr(uri, "//");
  ptr = ptr != NULL ? ptr + 2 : uri; // http:// 넘기기
  char *host_ptr = ptr;              // host 부분 찾기
  char *port_ptr = strchr(ptr, ':'); // port 부분 찾기
  char *path_ptr = strchr(ptr, '/'); // path 부분 찾기

  // 포트가 있는 경우
  if (port_ptr != NULL && (path_ptr == NULL || port_ptr < path_ptr))
  {
    strncpy(host, host_ptr, port_ptr - host_ptr); // 버퍼, 복사할 문자열, 복사할 길이
    strncpy(port, port_ptr + 1, path_ptr - port_ptr - 1);
  }
  // 포트가 없는 경우 80 으로 설정
  else
  {
    strcpy(port, "80");
    strncpy(host, host_ptr, path_ptr - host_ptr);
  }
  strcpy(path, path_ptr);
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