#include "Socket.h"
#include  <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int Socket(int domain, int type, int protocol) {
    int ret = socket(domain, type, protocol);
    if (ret < 0) {
        perror("socket");
        exit(-1);
    }
    return ret;
}

int Bind(int fd, const struct sockaddr *addr, socklen_t len) {
    int ret = bind(fd, addr, len);
    if (ret < 0) {
        perror("bind");
        exit(-1);
    }
    return ret;
}

ssize_t Recvfrom(int fd, void *__restrict buf, size_t n,
                 int flags, struct sockaddr *addr,
                 socklen_t *__restrict addr_len) {
    ssize_t ret = recvfrom(fd, buf, n, flags, addr, addr_len);
    if (ret < 0) {
        perror("recfrom");
        exit(-1);
    }
    return ret;
}

int Close(int fd) {
    int ret = close(fd);
    if (ret < 0) {
        perror("close");
        exit(-1);
    }
    return ret;
}

ssize_t Sendto(int fd, const void *buf, size_t n,
               int flags, const struct sockaddr *addr,
               socklen_t addr_len) {
    int ret = sendto(fd, buf, n, flags, addr, addr_len);
    if (ret < 0) {
        perror("sendto");
        exit(-1);
    }
    return ret;
}

int Connect(int fd, const struct sockaddr *addr, socklen_t len) {
    int ret = connect(fd, addr, len);
    if (ret < 0) {
        perror("connect");
        exit(-1);
    }
    return ret;
}

ssize_t Recv(int fd, void *buf, size_t n, int flags) {
    ssize_t ret = recv(fd, buf, n, flags);
    if (ret < 0) {
        perror("recv");
        exit(-1);
    }
    return ret;
}

ssize_t Send(int fd, const void *buf, size_t n, int flags) {
    ssize_t ret = send(fd, buf, n, flags);
    if (ret < 0) {
        perror("send");
        exit(-1);
    }
    return ret;
}

int Select(int nfds, fd_set *readfds,
           fd_set *writefds,
           fd_set *exceptfds,
           struct timeval *timeout) {
    int ret = select(nfds, readfds, writefds, exceptfds, timeout);
    if (ret == -1) {
        perror("select");
        exit(-1);
    }
    return ret;
}

int Shutdown(int fd, int how) {
    int ret = shutdown(fd, how);
    if (ret == -1) {
        perror("shutdown");
        exit(-1);
    }
    return ret;
}

ssize_t Read(int fd, void *buf, size_t count) {
    ssize_t ret = read(fd, buf, count);
    if (ret == -1) {
        perror("read");
        exit(-1);
    }
    return ret;
}

int Listen(int fd, int backlog) {
    int ret = listen(fd, backlog);
    if (ret == -1) {
        perror("listen");
        exit(-1);
    }
    return ret;
}

int Accept(int fd,
           struct sockaddr *addr, socklen_t *len) {
    int ret = accept(fd, addr, len);
    if (ret == -1) {
        perror("accept");
        exit(-1);
    }
    return ret;
}


int Setsockopt(int fd, int level, int optname, const void *optval, socklen_t optlen) {
    int ret = setsockopt(fd, level, optname, optval, optlen);
    if (ret < 0) {
        perror("setsockopt");
        exit(-1);
    }
    return ret;
}

int Getsockopt(int fd, int level, int optname, void *optval, socklen_t *optlen) {
    int ret = getsockopt(fd, level, optname, optval, optlen);
    if (ret < 0) {
        perror("getsockopt");
        exit(-1);
    }
    return ret;
}

int Inet_pton(int family, const char *strptr, void *addrptr) {
    int ret = inet_pton(family, strptr, addrptr);
    if (ret == -1) {
        perror("inet_pton");
        exit(-1);
    }
    return ret;
}

int Pthread_create(pthread_t *tid,
                   const pthread_attr_t *attr,
                   void *(*f)(void *),
                   void *arg) {
    int ret = pthread_create(tid, attr, f, arg);
    if (ret == -1) {
        perror("pthread");
        exit(-1);
    }
    return ret;
}

struct hostent *Gethostbyname
        (const char *hostname) {
    struct hostent *ret = gethostbyname(hostname);
    if (ret == NULL) {
        perror("gethostbyname");
        exit(-1);
    }
    return ret;
}

struct hostent *Gethostbyname2
        (const char *hostname, int
        family) {
    struct hostent *ret = gethostbyname2(hostname, family);
    if (ret == NULL) {
        perror("gethostbyname2");
        exit(-1);
    }
    return ret;

}

struct servent *Getservbyname
        (const char *servname,
         const char *protoname) {
    struct servent *ret = getservbyname(servname, protoname);
    if (ret == NULL) {
        perror("getservbyname");
        exit(-1);
    }
    return ret;


}

int Getaddrinfo(const char *hostname,
                const char *servname,
                const struct addrinfo *hints,
                struct addrinfo **res) {
    int ret = getaddrinfo(hostname, servname, hints, res);
    if (ret == -1) {
        perror("getaddrinfo");
        exit(-1);
    }
    return ret;
}

int Getnameinfo(const struct sockaddr *addr, socklen_t addrlen,
                char *host, socklen_t hostlen, char *serv, socklen_t servlen, int flags) {
    int ret = getnameinfo(addr, addrlen, host, hostlen, serv, servlen, flags);
    if (ret == -1) {
        perror("getnameinfo");
        exit(-1);
    }
    return ret;
}
int Getsockname(int fd,struct sockaddr* addr,socklen_t* len){
        int ret = getsockname(fd,addr,len);
        if (ret == -1) {
            perror("getsockname");
            exit(-1);
        }
        return ret;

}
