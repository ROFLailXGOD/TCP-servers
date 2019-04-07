#include <sys/socket.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <unistd.h>
#include <iostream>
#include <unordered_set>

#define MAXLINE 256

static std::unordered_set<int> fds;

static void *connection_handler(void *);

int main()
{
    int listenfd, connfd;
    struct sockaddr_in servaddr;

    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket() error");
        exit(1);
    }
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(54001);
    int opt = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof (opt));

    if (bind(listenfd, (sockaddr*)&servaddr, sizeof(servaddr)) == -1)
    {
        perror("bind() error");
        exit(1);
    }

    if (listen(listenfd, SOMAXCONN) == -1)
    {
        perror("listen() error");
        exit(1);
    }

    pthread_t thread;

    for (;;)
    {
        if ((connfd = accept(listenfd, nullptr, nullptr)) == -1)
        {
            perror("accept() error");
            exit(1);
        }
        else
        {
            fds.insert(connfd);
        }

        if (pthread_create(&thread, nullptr, &connection_handler, (void*)&connfd) < 0)
        {
            perror("pthread_create() error");
            exit(1);
        }
    }
}

size_t writen(int sockfd, const char *buf, size_t count)
{
    const char *p;
    size_t n;
    ssize_t rc;

    if (buf == nullptr)
    {
        errno = EFAULT;
        perror("writen() error");
        exit(1);
    }

    p = buf;
    n = count;
    while (n)
    {
        if ((rc = write(sockfd, p, n)) == -1)
        {
            perror("write() error");
            exit(1);
        }
        n -= rc;
        p += rc;
    }

    return count;
}

static void *connection_handler(void *arg)
{
    if(pthread_detach(pthread_self()) < 0)
    {
        perror("pthread_detach error");
        exit(1);
    }

    ssize_t n;
    char buf[MAXLINE];

    int sockfd = *((int *)arg);
    for (;;)
    {
        if ((n = read(sockfd, buf, MAXLINE)) <= 0)
            break;
        for (const auto&elem: fds)
        {
            if (elem != sockfd)
            {
                if (writen(elem, buf, n) == -1)
                {
                    n = -1;
                    break;
                }
            }
        }
    }

    if (close(sockfd) == -1)
    {
        perror("close() error");
        exit(1);
    }
    else {
        fds.erase(sockfd);
    }

    return nullptr;
}
