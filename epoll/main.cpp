#include <sys/socket.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <list>
#include <sys/epoll.h>

#define MAXLINE 256

std::list<int> clients;

int main()
{
    struct epoll_event ev;
    ev.events = EPOLLIN;

    char buff[1024];

    int pollfd;
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
    servaddr.sin_port = htons(54010);
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

    if((pollfd = epoll_create1(0)) == -1)
    {
        perror("epoll_create1() error");
        exit(1);
    }

    ev.data.fd = listenfd;
    if(epoll_ctl(pollfd, EPOLL_CTL_ADD, listenfd, &ev))
    {
        perror("epoll_ctl() error");
        exit(1);
    }

    for (;;)
    {
        int fd;
        int count = epoll_wait(pollfd, &ev, 1, 1000);
        if (count == -1)
        {
            perror("epol_wait() error");
            exit(1);
        }
        else if (count == 0)
            continue;

        fd = ev.data.fd;
        if (fd == listenfd)
        {
            if ((connfd = accept(listenfd, nullptr, nullptr)) == -1)
            {
                perror("accept() error");
                exit(1);
            }

            ev.data.fd = connfd;
            if (epoll_ctl(pollfd, EPOLL_CTL_ADD, connfd, &ev))
            {
                perror("epoll_ctl() error");
                exit(1);
            }
            clients.push_back(connfd);
        }
        else
        {
            int bytes = recv(fd, buff, sizeof (buff), 0);

            if (bytes == -1)
            {
                perror("recv() error");
                exit(1);
            }
            else if (bytes == 0)
            {
                if (epoll_ctl(pollfd, EPOLL_CTL_DEL, fd, nullptr))
                {
                    perror("epoll_ctl() error");
                    exit(1);
                }
                close(fd);
                clients.remove(fd);
                std::cout << "Connection closed";
                continue;
            }

            std::list<int>::iterator it;
            for (it = clients.begin(); it != clients.end(); ++it)
            {
                if (*it != fd)
                {
                    if ((send(*it, buff, bytes, 0)) == -1)
                    {
                             perror("send() error");
                             exit(1);
                    }
                }
            }
        }
    }
}
