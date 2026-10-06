#ifndef NETZWERKPROGRAMMIERUNG_SOCKET_H
#define NETZWERKPROGRAMMIERUNG_SOCKET_H

#include <sys/socket.h>
#include <sys/select.h>
#include <pthread.h>
#include <netdb.h>


int Socket(int domain, int type, int protocol);

int Bind(int fd, const struct sockaddr *addr, socklen_t len);

ssize_t Recvfrom(int fd, void *__restrict buf, size_t n,
                 int flags, struct sockaddr *addr,
                 socklen_t *__restrict addr_len);

int Close(int fd);

ssize_t Sendto(int fd, const void *buf, size_t n,
               int flags, const struct sockaddr *addr,
               socklen_t addr_len);

int Connect(int fd, const struct sockaddr *addr, socklen_t len);

ssize_t Recv(int fd, void *buf, size_t n, int flags);

ssize_t Send(int fd, const void *buf, size_t n, int flags);

int Select(int nfds, fd_set *readfds,
           fd_set *writefds,
           fd_set *exceptfds,
           struct timeval *timeout);

int Shutdown(int fd, int how);

ssize_t Read(int fd, void *buf, size_t count);

int Listen(int fd, int backlog);

int Accept(int fd,
           struct sockaddr *addr, socklen_t *len);

int Setsockopt(int fd, int level, int optname, const void *optval, socklen_t optlen);

int Getsockopt(int fd, int level, int optname, void *optval, socklen_t *optlen);

int Inet_pton(int family, const char *strptr, void *addrptr);

int Pthread_create(pthread_t *tid,
                   const pthread_attr_t *attr,
                   void *(*f)(void *),
                   void *arg);

struct hostent *Gethostbyname
        (const char *hostname);

struct hostent *Gethostbyname2
        (const char *hostname, int
        family);

struct servent *Getservbyname
        (const char *servname,
         const char *protoname);

int Getaddrinfo(const char *hostname,
                const char *servname,
                const struct addrinfo *hints,
                struct addrinfo **res);

int Getnameinfo(const struct sockaddr *addr, socklen_t addrlen,
                char *host, socklen_t hostlen, char *serv, socklen_t servlen, int flags);

int Getsockname(int fd,struct sockaddr* addr,socklen_t* len);

#endif //NETZWERKPROGRAMMIERUNG_SOCKET_H
