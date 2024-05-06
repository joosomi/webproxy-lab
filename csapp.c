/* 
 * csapp.c - Functions for the CS:APP3e book
 *
 * Updated 10/2016 reb:
 *   - Fixed bug in sio_ltoa that didn't cover negative numbers
 *
 * Updated 2/2016 droh:
 *   - Updated open_clientfd and open_listenfd to fail more gracefully
 *
 * Updated 8/2014 droh: 
 *   - New versions of open_clientfd and open_listenfd are reentrant and
 *     protocol independent.
 *
 *   - Added protocol-independent inet_ntop and inet_pton functions. The
 *     inet_ntoa and inet_aton functions are obsolete.
 *
 * Updated 7/2014 droh:
 *   - Aded reentrant sio (signal-safe I/O) routines
 * 
 * Updated 4/2013 droh: 
 *   - rio_readlineb: fixed edge case bug
 *   - rio_readnb: removed redundant EINTR check
 */
/* $begin csapp.c */
#include "csapp.h"

/************************** 
 * Error-handling functions
 **************************/
/* $begin errorfuns */
/* $begin unixerror */
void unix_error(char *msg) /* Unix-style error */
{
    fprintf(stderr, "%s: %s\n", msg, strerror(errno));
    exit(0);
}
/* $end unixerror */

void posix_error(int code, char *msg) /* Posix-style error */
{
    fprintf(stderr, "%s: %s\n", msg, strerror(code));
    exit(0);
}

void gai_error(int code, char *msg) /* Getaddrinfo-style error */
{
    fprintf(stderr, "%s: %s\n", msg, gai_strerror(code));
    exit(0);
}

void app_error(char *msg) /* Application error */
{
    fprintf(stderr, "%s\n", msg);
    exit(0);
}
/* $end errorfuns */

void dns_error(char *msg) /* Obsolete gethostbyname error */
{
    fprintf(stderr, "%s\n", msg);
    exit(0);
}


/*********************************************
 * Wrappers for Unix process control functions
 ********************************************/

/* $begin forkwrapper */
pid_t Fork(void) 
{
    pid_t pid;

    if ((pid = fork()) < 0)
	unix_error("Fork error");
    return pid;
}
/* $end forkwrapper */

void Execve(const char *filename, char *const argv[], char *const envp[]) 
{
    if (execve(filename, argv, envp) < 0)
	unix_error("Execve error");
}

/* $begin wait */
pid_t Wait(int *status) 
{
    pid_t pid;

    if ((pid  = wait(status)) < 0)
	unix_error("Wait error");
    return pid;
}
/* $end wait */

pid_t Waitpid(pid_t pid, int *iptr, int options) 
{
    pid_t retpid;

    if ((retpid  = waitpid(pid, iptr, options)) < 0) 
	unix_error("Waitpid error");
    return(retpid);
}

/* $begin kill */
void Kill(pid_t pid, int signum) 
{
    int rc;

    if ((rc = kill(pid, signum)) < 0)
	unix_error("Kill error");
}
/* $end kill */

void Pause() 
{
    (void)pause();
    return;
}

unsigned int Sleep(unsigned int secs) 
{
    unsigned int rc;

    if ((rc = sleep(secs)) < 0)
	unix_error("Sleep error");
    return rc;
}

unsigned int Alarm(unsigned int seconds) {
    return alarm(seconds);
}
 
void Setpgid(pid_t pid, pid_t pgid) {
    int rc;

    if ((rc = setpgid(pid, pgid)) < 0)
	unix_error("Setpgid error");
    return;
}

pid_t Getpgrp(void) {
    return getpgrp();
}

/************************************
 * Wrappers for Unix signal functions 
 ***********************************/

/* $begin sigaction */
handler_t *Signal(int signum, handler_t *handler) 
{
    struct sigaction action, old_action;

    action.sa_handler = handler;  
    sigemptyset(&action.sa_mask); /* Block sigs of type being handled */
    action.sa_flags = SA_RESTART; /* Restart syscalls if possible */

    if (sigaction(signum, &action, &old_action) < 0)
	unix_error("Signal error");
    return (old_action.sa_handler);
}
/* $end sigaction */

void Sigprocmask(int how, const sigset_t *set, sigset_t *oldset)
{
    if (sigprocmask(how, set, oldset) < 0)
	unix_error("Sigprocmask error");
    return;
}

void Sigemptyset(sigset_t *set)
{
    if (sigemptyset(set) < 0)
	unix_error("Sigemptyset error");
    return;
}

void Sigfillset(sigset_t *set)
{ 
    if (sigfillset(set) < 0)
	unix_error("Sigfillset error");
    return;
}

void Sigaddset(sigset_t *set, int signum)
{
    if (sigaddset(set, signum) < 0)
	unix_error("Sigaddset error");
    return;
}

void Sigdelset(sigset_t *set, int signum)
{
    if (sigdelset(set, signum) < 0)
	unix_error("Sigdelset error");
    return;
}

int Sigismember(const sigset_t *set, int signum)
{
    int rc;
    if ((rc = sigismember(set, signum)) < 0)
	unix_error("Sigismember error");
    return rc;
}

int Sigsuspend(const sigset_t *set)
{
    int rc = sigsuspend(set); /* always returns -1 */
    if (errno != EINTR)
        unix_error("Sigsuspend error");
    return rc;
}

/*************************************************************
 * The Sio (Signal-safe I/O) package - simple reentrant output
 * functions that are safe for signal handlers.
 *************************************************************/

/* Private sio functions */

/* $begin sioprivate */
/* sio_reverse - Reverse a string (from K&R) */
static void sio_reverse(char s[])
{
    int c, i, j;

    for (i = 0, j = strlen(s)-1; i < j; i++, j--) {
        c = s[i];
        s[i] = s[j];
        s[j] = c;
    }
}

/* sio_ltoa - Convert long to base b string (from K&R) */
static void sio_ltoa(long v, char s[], int b) 
{
    int c, i = 0;
    int neg = v < 0;

    if (neg)
	v = -v;

    do {  
        s[i++] = ((c = (v % b)) < 10)  ?  c + '0' : c - 10 + 'a';
    } while ((v /= b) > 0);

    if (neg)
	s[i++] = '-';

    s[i] = '\0';
    sio_reverse(s);
}

/* sio_strlen - Return length of string (from K&R) */
static size_t sio_strlen(char s[])
{
    int i = 0;

    while (s[i] != '\0')
        ++i;
    return i;
}
/* $end sioprivate */

/* Public Sio functions */
/* $begin siopublic */

ssize_t sio_puts(char s[]) /* Put string */
{
    return write(STDOUT_FILENO, s, sio_strlen(s)); //line:csapp:siostrlen
}

ssize_t sio_putl(long v) /* Put long */
{
    char s[128];
    
    sio_ltoa(v, s, 10); /* Based on K&R itoa() */  //line:csapp:sioltoa
    return sio_puts(s);
}

void sio_error(char s[]) /* Put error message and exit */
{
    sio_puts(s);
    _exit(1);                                      //line:csapp:sioexit
}
/* $end siopublic */

/*******************************
 * Wrappers for the SIO routines
 ******************************/
ssize_t Sio_putl(long v)
{
    ssize_t n;
  
    if ((n = sio_putl(v)) < 0)
	sio_error("Sio_putl error");
    return n;
}

ssize_t Sio_puts(char s[])
{
    ssize_t n;
  
    if ((n = sio_puts(s)) < 0)
	sio_error("Sio_puts error");
    return n;
}

void Sio_error(char s[])
{
    sio_error(s);
}

/********************************
 * Wrappers for Unix I/O routines
 ********************************/

int Open(const char *pathname, int flags, mode_t mode) 
{
    int rc;

    if ((rc = open(pathname, flags, mode))  < 0)
	unix_error("Open error");
    return rc;
}

ssize_t Read(int fd, void *buf, size_t count) 
{
    ssize_t rc;

    if ((rc = read(fd, buf, count)) < 0) 
	unix_error("Read error");
    return rc;
}

ssize_t Write(int fd, const void *buf, size_t count) 
{
    ssize_t rc;

    if ((rc = write(fd, buf, count)) < 0)
	unix_error("Write error");
    return rc;
}

off_t Lseek(int fildes, off_t offset, int whence) 
{
    off_t rc;

    if ((rc = lseek(fildes, offset, whence)) < 0)
	unix_error("Lseek error");
    return rc;
}

void Close(int fd) 
{
    int rc;

    if ((rc = close(fd)) < 0)
	unix_error("Close error");
}

int Select(int  n, fd_set *readfds, fd_set *writefds,
	   fd_set *exceptfds, struct timeval *timeout) 
{
    int rc;

    if ((rc = select(n, readfds, writefds, exceptfds, timeout)) < 0)
	unix_error("Select error");
    return rc;
}

int Dup2(int fd1, int fd2) 
{
    int rc;

    if ((rc = dup2(fd1, fd2)) < 0)
	unix_error("Dup2 error");
    return rc;
}

void Stat(const char *filename, struct stat *buf) 
{
    if (stat(filename, buf) < 0)
	unix_error("Stat error");
}

void Fstat(int fd, struct stat *buf) 
{
    if (fstat(fd, buf) < 0)
	unix_error("Fstat error");
}

/*********************************
 * Wrappers for directory function
 *********************************/

DIR *Opendir(const char *name) 
{
    DIR *dirp = opendir(name); 

    if (!dirp)
        unix_error("opendir error");
    return dirp;
}

struct dirent *Readdir(DIR *dirp)
{
    struct dirent *dep;
    
    errno = 0;
    dep = readdir(dirp);
    if ((dep == NULL) && (errno != 0))
        unix_error("readdir error");
    return dep;
}

int Closedir(DIR *dirp) 
{
    int rc;

    if ((rc = closedir(dirp)) < 0)
        unix_error("closedir error");
    return rc;
}

/***************************************
 * Wrappers for memory mapping functions
 ***************************************/
void *Mmap(void *addr, size_t len, int prot, int flags, int fd, off_t offset) 
{
    void *ptr;

    if ((ptr = mmap(addr, len, prot, flags, fd, offset)) == ((void *) -1))
	unix_error("mmap error");
    return(ptr);
}

void Munmap(void *start, size_t length) 
{
    if (munmap(start, length) < 0)
	unix_error("munmap error");
}

/***************************************************
 * Wrappers for dynamic storage allocation functions
 ***************************************************/

void *Malloc(size_t size) 
{
    void *p;

    if ((p  = malloc(size)) == NULL)
	unix_error("Malloc error");
    return p;
}

void *Realloc(void *ptr, size_t size) 
{
    void *p;

    if ((p  = realloc(ptr, size)) == NULL)
	unix_error("Realloc error");
    return p;
}

void *Calloc(size_t nmemb, size_t size) 
{
    void *p;

    if ((p = calloc(nmemb, size)) == NULL)
	unix_error("Calloc error");
    return p;
}

void Free(void *ptr) 
{
    free(ptr);
}

/******************************************
 * Wrappers for the Standard I/O functions.
 ******************************************/
void Fclose(FILE *fp) 
{
    if (fclose(fp) != 0)
	unix_error("Fclose error");
}

FILE *Fdopen(int fd, const char *type) 
{
    FILE *fp;

    if ((fp = fdopen(fd, type)) == NULL)
	unix_error("Fdopen error");

    return fp;
}

char *Fgets(char *ptr, int n, FILE *stream) 
{
    char *rptr;

    if (((rptr = fgets(ptr, n, stream)) == NULL) && ferror(stream))
	app_error("Fgets error");

    return rptr;
}

FILE *Fopen(const char *filename, const char *mode) 
{
    FILE *fp;

    if ((fp = fopen(filename, mode)) == NULL)
	unix_error("Fopen error");

    return fp;
}

void Fputs(const char *ptr, FILE *stream) 
{
    if (fputs(ptr, stream) == EOF)
	unix_error("Fputs error");
}

size_t Fread(void *ptr, size_t size, size_t nmemb, FILE *stream) 
{
    size_t n;

    if (((n = fread(ptr, size, nmemb, stream)) < nmemb) && ferror(stream)) 
	unix_error("Fread error");
    return n;
}

void Fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream) 
{
    if (fwrite(ptr, size, nmemb, stream) < nmemb)
	unix_error("Fwrite error");
}


/**************************** 
 * Sockets interface wrappers
 ****************************/

int Socket(int domain, int type, int protocol) 
{
    int rc;

    if ((rc = socket(domain, type, protocol)) < 0)
	unix_error("Socket error");
    return rc;
}

void Setsockopt(int s, int level, int optname, const void *optval, int optlen) 
{
    int rc;

    if ((rc = setsockopt(s, level, optname, optval, optlen)) < 0)
	unix_error("Setsockopt error");
}

void Bind(int sockfd, struct sockaddr *my_addr, int addrlen) 
{
    int rc;

    if ((rc = bind(sockfd, my_addr, addrlen)) < 0)
	unix_error("Bind error");
}

void Listen(int s, int backlog) 
{
    int rc;

    if ((rc = listen(s,  backlog)) < 0)
	unix_error("Listen error");
}

int Accept(int s, struct sockaddr *addr, socklen_t *addrlen) 
{
    int rc;

    if ((rc = accept(s, addr, addrlen)) < 0)
	unix_error("Accept error");
    return rc;
}

void Connect(int sockfd, struct sockaddr *serv_addr, int addrlen) 
{
    int rc;

    if ((rc = connect(sockfd, serv_addr, addrlen)) < 0)
	unix_error("Connect error");
}

/*******************************
 * Protocol-independent wrappers
 *******************************/
/* $begin getaddrinfo */
void Getaddrinfo(const char *node, const char *service, 
                 const struct addrinfo *hints, struct addrinfo **res)
{
    int rc;

    if ((rc = getaddrinfo(node, service, hints, res)) != 0) 
        gai_error(rc, "Getaddrinfo error");
}
/* $end getaddrinfo */

void Getnameinfo(const struct sockaddr *sa, socklen_t salen, char *host, 
                 size_t hostlen, char *serv, size_t servlen, int flags)
{
    int rc;

    if ((rc = getnameinfo(sa, salen, host, hostlen, serv, 
                          servlen, flags)) != 0) 
        gai_error(rc, "Getnameinfo error");
}

void Freeaddrinfo(struct addrinfo *res)
{
    freeaddrinfo(res);
}

void Inet_ntop(int af, const void *src, char *dst, socklen_t size)
{
    if (!inet_ntop(af, src, dst, size))
        unix_error("Inet_ntop error");
}

void Inet_pton(int af, const char *src, void *dst) 
{
    int rc;

    rc = inet_pton(af, src, dst);
    if (rc == 0)
	app_error("inet_pton error: invalid dotted-decimal address");
    else if (rc < 0)
        unix_error("Inet_pton error");
}

/*******************************************
 * DNS interface wrappers. 
 *
 * NOTE: These are obsolete because they are not thread safe. Use
 * getaddrinfo and getnameinfo instead
 ***********************************/

/* $begin gethostbyname */
struct hostent *Gethostbyname(const char *name) 
{
    struct hostent *p;

    if ((p = gethostbyname(name)) == NULL)
	dns_error("Gethostbyname error");
    return p;
}
/* $end gethostbyname */

struct hostent *Gethostbyaddr(const char *addr, int len, int type) 
{
    struct hostent *p;

    if ((p = gethostbyaddr(addr, len, type)) == NULL)
	dns_error("Gethostbyaddr error");
    return p;
}

/************************************************
 * Wrappers for Pthreads thread control functions
 ************************************************/

void Pthread_create(pthread_t *tidp, pthread_attr_t *attrp, 
		    void * (*routine)(void *), void *argp) 
{
    int rc;

    if ((rc = pthread_create(tidp, attrp, routine, argp)) != 0)
	posix_error(rc, "Pthread_create error");
}

void Pthread_cancel(pthread_t tid) {
    int rc;

    if ((rc = pthread_cancel(tid)) != 0)
	posix_error(rc, "Pthread_cancel error");
}

void Pthread_join(pthread_t tid, void **thread_return) {
    int rc;

    if ((rc = pthread_join(tid, thread_return)) != 0)
	posix_error(rc, "Pthread_join error");
}

/* $begin detach */
void Pthread_detach(pthread_t tid) {
    int rc;

    if ((rc = pthread_detach(tid)) != 0)
	posix_error(rc, "Pthread_detach error");
}
/* $end detach */

void Pthread_exit(void *retval) {
    pthread_exit(retval);
}

pthread_t Pthread_self(void) {
    return pthread_self();
}
 
void Pthread_once(pthread_once_t *once_control, void (*init_function)()) {
    pthread_once(once_control, init_function);
}

/*******************************
 * Wrappers for Posix semaphores
 *******************************/

void Sem_init(sem_t *sem, int pshared, unsigned int value) 
{
    if (sem_init(sem, pshared, value) < 0)
	unix_error("Sem_init error");
}

void P(sem_t *sem) 
{
    if (sem_wait(sem) < 0)
	unix_error("P error");
}

void V(sem_t *sem) 
{
    if (sem_post(sem) < 0)
	unix_error("V error");
}

/****************************************
 * The Rio package - Robust I/O functions
 ****************************************/

/*
 * rio_readn - Robustly read n bytes (unbuffered)
 */
/* $begin rio_readn */

/*현재 파일 식별자 fd에서 n만큼의 데이터를 버퍼 usrbuf로 보냄
-> 데이터가 분할되어 도착하거나 시그널에 의해 읽기가 중단되는 상황에서도 안정적으로 데이터 읽을 수 있도록  */
ssize_t rio_readn(int fd, void *usrbuf, size_t n) 
{
    size_t nleft = n; //읽어야할 바이트 수 
    ssize_t nread; //실제로 읽은 바이트 수 
    char *bufp = usrbuf; //사용자 버퍼의 현재 위치를 가리키는 포인터

    //남은 바이트가 0보다 크면 계속 읽음
    while (nleft > 0) { 
    //파일 디스크립터로부터 nleft만큼 읽기
	if ((nread = read(fd, bufp, nleft)) < 0) {
        //read(0 호출이 시그널에 의해 중단될 경우
	    if (errno == EINTR) /* Interrupted by sig handler return */
		    nread = 0;      /* and call read() again 다시 read()호출하기 위해 nread 0으로 설정*/
	    else
		    return -1;      /* errno set by read(), 다른 오류가 발생한 경우 -1 반환 */ 
	} 
	else if (nread == 0)
	    break;              /* EOF(파일끝에 도달) -> 반복 종료*/
    
	nleft -= nread; //읽은 바이트만큼 남은 바이트  수 감소
	bufp += nread; // 버퍼 포인터를 이동
    }
    return (n - nleft);         /* Return >= 0 
                요청한 바이트 수에서 남은 바이트 수를 빼서 실제로 읽은 바이트 수 반환*/
}
/* $end rio_readn */




/*
 * rio_writen - Robustly write n bytes (unbuffered)
 현재 메모리의 버퍼 usrbuf에서 n만큼의 데이터를 파일 식별자 fd로 보냄
 */
/* $begin rio_writen */
ssize_t rio_writen(int fd, void *usrbuf, size_t n) 
{
    size_t nleft = n; //쓰기를 완료해야 하는 전체 바이트 수
    ssize_t nwritten; //실제로 쓰여진 바이트 수 
    char *bufp = usrbuf; //사용자 버퍼의 포인터

    //남은 바이트가 0보다 클 때 반복
    while (nleft > 0) {
	if ((nwritten = write(fd, bufp, nleft)) <= 0) {
	    if (errno == EINTR)  /* Interrupted by sig handler return 시그널에 의해 쓰기가 중단된 경우 */
		    nwritten = 0;    /* and call write() again, 다시 쓰기를 시도 */
	    else
		    return -1;       /* errno set by write(), 쓰기 오류 발생 => -1 반환 */
	}
	nleft -= nwritten; //쓴 바이트만큼 남은 바이트 수 감소시킴
	bufp += nwritten; //버퍼 포인터 이동
    }
    return n; //전송한 바이트 수 반환
}
/* $end rio_writen */


/* 
 * rio_read - This is a wrapper for the Unix read() function that
 *    transfers min(n, rio_cnt) bytes from an internal buffer to a user
 *    buffer, where n is the number of bytes requested by the user and
 *    rio_cnt is the number of unread bytes in the internal buffer. On
 *    entry, rio_read() refills the internal buffer via a call to
 *    read() if the internal buffer is empty.
 *  
 *  내부 버퍼에서 사용자 버퍼로 min(n, rio_cnt) 바이트 전송
 *  n: 사용자가 요청한 바이트 수,
 *  rio_cnt : 내부 버퍼에 남아 있는, 읽지 않는 바이트 수 
 *  호출 시에 내부 버퍼가 비어있으면 rio_read()는 read() 호출을 통해 내부 버퍼 채움
 */
/* $begin rio_read */
static ssize_t rio_read(rio_t *rp, char *usrbuf, size_t n)
{
    int cnt; //실제로 사용자 버퍼로 복사되는 바이트 수 

    //내부 버퍼에 남아있는 바이트의 수 <= 0이면 -> read() 함수를 통해 내부 버퍼를 다시 채운다.
    while (rp->rio_cnt <= 0) {  /* Refill if buf is empty */
	rp->rio_cnt = read(rp->rio_fd, rp->rio_buf, 
			   sizeof(rp->rio_buf));
	if (rp->rio_cnt < 0) {
	    if (errno != EINTR) /* Interrupted by sig handler return */
		return -1;
	}
	else if (rp->rio_cnt == 0)  /* EOF, 파일의 끝*/
	    return 0; // 더이상 읽을 데이터가 없음
	else 
	    rp->rio_bufptr = rp->rio_buf; /* Reset buffer ptr, 버퍼 포인터 리셋 */
    }

    /* 내부 버퍼에서 사용자 버퍼로 min(n, rp->rio_cnt) 바이트 복사*/
    /* Copy min(n, rp->rio_cnt) bytes from internal buf to user buf */
    cnt = n;          

    /*사용자가 요청한 바이트 수와 내부 버퍼에 남은 바이트 수 중 작은 값 선택*/
    if (rp->rio_cnt < n)   
	    cnt = rp->rio_cnt; 
    memcpy(usrbuf, rp->rio_bufptr, cnt);// 데이터 복사
    rp->rio_bufptr += cnt; // 내부 버퍼 포인터 업데이트
    rp->rio_cnt -= cnt; //남은 바이트 수 업데이트
    return cnt; //전송된 바이트 수 반환
}
/* $end rio_read */

/*
 * rio_readinitb - Associate a descriptor with a read buffer and reset buffer
 파일 디스크립터를 읽기 버퍼와 연결하고 버퍼를 초기화 
 */
/* $begin rio_readinitb */
void rio_readinitb(rio_t *rp, int fd) 
{
    rp->rio_fd = fd;  //파일 디스크립터 설정
    rp->rio_cnt = 0;  // rio_cnt :내부 버퍼에 남아있는 바이트 수 => 0으로 초기화 
    rp->rio_bufptr = rp->rio_buf; //버퍼 포인터를 내부 버퍼의 시작 위치로 설정. 
    // 데이터 읽기 시작할 위치를 내부 버퍼의 처음으로 지정
}
/* $end rio_readinitb */


/*
 * rio_readnb - Robustly read n bytes (buffered)
 */
/* $begin rio_readnb */
ssize_t rio_readnb(rio_t *rp, void *usrbuf, size_t n) 
{
    size_t nleft = n; //읽어야할 남아있는 바이트 수 
    ssize_t nread; //실제로 읽은 바이트 수
    char *bufp = usrbuf; //bufp 포인터 - 사용자 버퍼의 시작 주소 가리킴
    
    //남은 바이트 수가 0보다 클 동안 반복
    while (nleft > 0) {
	if ((nread = rio_read(rp, bufp, nleft)) < 0) 
            return -1;          /* errno set by read() */ 
	else if (nread == 0)
	    break;              /* EOF에 도달하면 반복 종료 */
	nleft -= nread; // 남은 바이트 수 감소시킴
	bufp += nread; // 사용자 버퍼 포인터 업데이트
    }
    return (n - nleft);         /* return >= 0 
    성공적으로 읽은 바이트 수 반환, 오류가 없으면 0 이상의 값 반환*/
}
/* $end rio_readnb */

/* 
 * rio_readlineb - Robustly read a text line (buffered)
 */
/* $begin rio_readlineb 
한줄 단위로 입력 받음. 
rp: rio_t 구조체에 대한 포인터, 내부 버퍼와 관련된 메타데이터 관리
usrbuf: 사용자가 제공하는 버퍼, 읽은 데이터 저장
maxlen: 최대로 읽을 수 있는 바이트 수 - 버퍼의 크기
*/
ssize_t rio_readlineb(rio_t *rp, void *usrbuf, size_t maxlen) 
{
    int n, rc;
    char c, *bufp = usrbuf;

    for (n = 1; n < maxlen; n++) { 
        //rio_read()를 통해 문자 하나 읽고, 성공적으로 읽었으면 실행
        if ((rc = rio_read(rp, &c, 1)) == 1) {
	    *bufp++ = c; //읽은 문자를 사용자 버퍼에 저장
        // 만약 읽은 문자가 \n이면 
	    if (c == '\n') {
                n++; //읽은 바이트 수 하나 증가시키고 반복 종료
     		break;
        }
    //rio_read() 반환 값이 0이면 EOF에 도달한 것
	} else if (rc == 0) {
	    if (n == 1) //만약 아무 데이터도 읽지 않았으면
		return 0; /* EOF, no data read - 0반환, 데이터가 없음*/
	    else //데이터 일부를 읽었으면 반복 종료
		break;    /* EOF, some data was read */
	} else //rio_read에서 -1 반환하면 -> 오류가 발생한 것
	    return -1;	  /* Error */
    }
    *bufp = 0; //사용자 버퍼의 마지막에 문자열의 끝을 알리기위해 널 문자('\0') 추가
    return n-1; //실제로 읽은 바이트 수 정확히 반환하기 위해 읽은 바이트 수에서 1 빼서 반환
}
/* $end rio_readlineb */

/**********************************
 * Wrappers for robust I/O routines
 **********************************/
ssize_t Rio_readn(int fd, void *ptr, size_t nbytes) 
{
    ssize_t n;
  
    if ((n = rio_readn(fd, ptr, nbytes)) < 0)
	unix_error("Rio_readn error");
    return n;
}

void Rio_writen(int fd, void *usrbuf, size_t n) 
{
    if (rio_writen(fd, usrbuf, n) != n)
	unix_error("Rio_writen error");
}

void Rio_readinitb(rio_t *rp, int fd)
{
    rio_readinitb(rp, fd);
} 

ssize_t Rio_readnb(rio_t *rp, void *usrbuf, size_t n) 
{
    ssize_t rc;

    if ((rc = rio_readnb(rp, usrbuf, n)) < 0)
	unix_error("Rio_readnb error");
    return rc;
}

ssize_t Rio_readlineb(rio_t *rp, void *usrbuf, size_t maxlen) 
{
    ssize_t rc;

    if ((rc = rio_readlineb(rp, usrbuf, maxlen)) < 0)
	unix_error("Rio_readlineb error");
    return rc;
} 

/******************************** 
 * Client/server helper functions
 ********************************/
/*
 * open_clientfd - Open connection to server at <hostname, port> and
 *     return a socket descriptor ready for reading and writing. This
 *     function is reentrant and protocol-independent.
 *
 *     On error, returns: 
 *       -2 for getaddrinfo error
 *       -1 with errno set for other errors.
 */
/* $begin open_clientfd */
int open_clientfd(char *hostname, char *port) {
    int clientfd, rc;
    struct addrinfo hints, *listp, *p;

    /* 잠재적인 서버 주소 목록 가져옴*/
    /* Get a list of potential server addresses */
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_socktype = SOCK_STREAM;  /* Open a connection, 연결을 위한 소켓 타입 지정 */
    hints.ai_flags = AI_NUMERICSERV;  /* ... using a numeric port arg. 숫자 포트 인자 사용 */
    hints.ai_flags |= AI_ADDRCONFIG;  /* Recommended for connections, 연결에 권장되는 플래그 설정 */
    if ((rc = getaddrinfo(hostname, port, &hints, &listp)) != 0) {
        fprintf(stderr, "getaddrinfo failed (%s:%s): %s\n", hostname, port, gai_strerror(rc));
        return -2; //getaddrinfo 오류 시 -2 반환
    }
    
    //연결 가능한 주소 찾아서 순회
    /* Walk the list for one that we can successfully connect to */
    for (p = listp; p; p = p->ai_next) {
        /* Create a socket descriptor 소켓 디스크립터 생성 */
        if ((clientfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0) 
            continue; /* Socket failed, try the next, 소켓 생성 실패, 다음 주소 시도 */

        /* Connect to the server 서버에 연결 시도*/
        if (connect(clientfd, p->ai_addr, p->ai_addrlen) != -1) 
            break; /* Success 성공 시 반복 중단*/
        if (close(clientfd) < 0) { /* Connect failed, try another, 연결 실패 다른 주소 시도 */  //line:netp:openclientfd:closefd
            fprintf(stderr, "open_clientfd: close failed: %s\n", strerror(errno));
            return -1; //오류시 -1 반환
        } 
    } 

    /* Clean up */
    freeaddrinfo(listp);
    if (!p) /* All connects failed */
        return -1;
    else    /* The last connect succeeded */
        return clientfd;
}
/* $end open_clientfd */

/*  
 * open_listenfd - Open and return a listening socket on port. This
 *     function is reentrant and protocol-independent.
 *
 *     On error, returns: 
 *       -2 for getaddrinfo error
 *       -1 with errno set for other errors.
 */
/* $begin open_listenfd */
int open_listenfd(char *port) 
{
    struct addrinfo hints, *listp, *p;
    int listenfd, rc, optval=1;

    /* Get a list of potential server addresses 잠재적인 서버 주소 목록 가져옴*/
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_socktype = SOCK_STREAM;             /* Accept connections 연결 수락*/
    hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG; /* ... on any IP address */
    hints.ai_flags |= AI_NUMERICSERV;            /* ... using port number */
    if ((rc = getaddrinfo(NULL, port, &hints, &listp)) != 0) {
        fprintf(stderr, "getaddrinfo failed (port %s): %s\n", port, gai_strerror(rc));
        return -2;
    }

    //바인딩할 수 있는 주소를 리스트에서 찾음
    /* Walk the list for one that we can bind to */
    for (p = listp; p; p = p->ai_next) {
        /* Create a socket descriptor, 소켓 디스크립터 생성*/
        if ((listenfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0) 
            continue;  /* Socket failed, try the next, 소켓 생성 실패, 다음 주소로 시도 */

        //주소가 이미 사용중입니다 오류 방지
        /* Eliminates "Address already in use" error from bind */
        setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR,    //line:netp:csapp:setsockopt
                   (const void *)&optval , sizeof(int));

        //bind를 활용하여 포트번호를 ip주소에 묶음
        /* Bind the descriptor to the address */
        if (bind(listenfd, p->ai_addr, p->ai_addrlen) == 0)
            break; /* Success 성공, */
        if (close(listenfd) < 0) { /* Bind failed, try the next 바인드 실패 -> 다음 주소로 시도*/
            fprintf(stderr, "open_listenfd close failed: %s\n", strerror(errno));
            return -1;
        }
    }


    /* Clean up 정리 작업*/
    freeaddrinfo(listp);
    if (!p) /* No address worked 모든 주소 시도 실패*/
        return -1;

    /* Make it a listening socket ready to accept connection requests */
    // 듣기 소켓을 준비하여 연결 요청을 수락할 준비
    if (listen(listenfd, LISTENQ) < 0) {
        close(listenfd);
	return -1;
    }
    return listenfd;
}
/* $end open_listenfd */

/****************************************************
 * Wrappers for reentrant protocol-independent helpers
 ****************************************************/
int Open_clientfd(char *hostname, char *port) 
{
    int rc;

    if ((rc = open_clientfd(hostname, port)) < 0) 
	unix_error("Open_clientfd error");
    return rc;
}

int Open_listenfd(char *port) 
{
    int rc;

    if ((rc = open_listenfd(port)) < 0)
	unix_error("Open_listenfd error");
    return rc;
}

/* $end csapp.c */




