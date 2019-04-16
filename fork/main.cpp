#include <sys/socket.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <map>
#include <sys/epoll.h>
#include <fcntl.h>

#define MAXLINE 256

std::map<int, int> pipes;

size_t writen(int sockfd, const char *buf, size_t count);
int setnonblocking(int fd);

int main()
{
    struct epoll_event ev;

    char buff[4096];

    pid_t childpid;

    int pollfd;
    int listenfd, connfd;
    struct sockaddr_in servaddr;

    if ((listenfd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)) < 0)
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

    if((pollfd = epoll_create1(0)) == -1)
    {
        perror("epoll_create1() error");
        exit(1);
    }

    for (;;)
    {
        if ((connfd = accept(listenfd, nullptr, nullptr)) == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                int count = epoll_wait(pollfd, &ev, 1, 1000);
                if (count == -1)
                {
                    if (errno == EINTR)
                        continue;
                    perror("epol_wait() error");
                    exit(1);
                }
                else if (count == 0)
                    continue;


                //read incoming message
                int bytes = read(ev.data.fd, buff, sizeof(buff));

                if (bytes == -1)
                {
                    perror("recv() error");
                    exit(1);
                }
                else if (bytes == 0)
                {
                    if (epoll_ctl(pollfd, EPOLL_CTL_DEL, ev.data.fd, nullptr))
                    {
                        perror("epoll_ctl() error");
                        exit(1);
                    }
                    close(pipes[ev.data.fd]);
                    close(ev.data.fd);
                    pipes.erase(ev.data.fd);
                    continue;
                }


                //send to other pipes
                std::map<int,int>::iterator it;
                for (it = pipes.begin(); it != pipes.end(); ++it)
                {
                    if ((*it).first != ev.data.fd)
                    {
                        writen((*it).second, buff, bytes);
                    }
                }

                continue;
            }
            else
            {
                perror("accept() error");
                exit(1);
            }
        }

        if (setnonblocking(connfd) == -1)
        {
            perror("fcntl() error");
            exit(1);
        }
        int pr_pipefd[2];
        int cr_pipefd[2];
        if (pipe(pr_pipefd) == -1)
        {
            perror("pipe() error");
            exit(1);
        }
        if (pipe(cr_pipefd) == -1)
        {
            perror("pipe() error");
            exit(1);
        }
        if (setnonblocking(cr_pipefd[0]) == -1)
        {
            perror("fcntl() error");
            exit(1);
        }

        pipes.insert(std::pair<int,int>(pr_pipefd[0], cr_pipefd[1]));

        //add read end of the pipe to epoll
        ev.events = EPOLLIN | EPOLLET;
        ev.data.fd = pr_pipefd[0];
        if(epoll_ctl(pollfd, EPOLL_CTL_ADD, pr_pipefd[0], &ev))
        {
            perror("epoll_ctl() error");
            exit(1);
        }


        if ((childpid = fork()) == -1)
        {
            perror("fork() error");
            exit(1);
        }
        else if (childpid > 0) //parent
        {
            if (close(connfd) == -1)
            {
                perror("close() error");
                exit(1);
            }
            if (close(pr_pipefd[1]) == -1)
            {
                perror("close() error");
                exit(1);
            }
            if (close(cr_pipefd[0]) == -1)
            {
                perror("close() error");
                exit(1);
            }
        }
        else //child
        {
            if (close(listenfd) == -1)
            {
                perror("close() error");
                exit(1);
            }
            if (close(pr_pipefd[0]) == -1)
            {
                perror("close() error");
                exit(1);
            }
            if (close(cr_pipefd[1]) == -1)
            {
                perror("close() error");
                exit(1);
            }

            //receive message
            //send to pipe
            int bytes;
            for (;;)
            {
                if ((bytes = splice(connfd, nullptr, pr_pipefd[1], nullptr, 4096, SPLICE_F_MOVE | SPLICE_F_NONBLOCK)) <= 0)
                {
                    if (errno != EAGAIN || bytes == 0)
                        break;
                }

                if ((bytes = splice(cr_pipefd[0], nullptr, connfd, nullptr, 4096, SPLICE_F_MOVE | SPLICE_F_NONBLOCK)) <= 0)
                {
                    if (errno != EAGAIN)
                    {
                        perror("splice() error");
                        exit(1);
                    }
                }
            }

            if (close(connfd) == -1)
            {
                perror("close() error");
                exit(1);
            }
            if (close(cr_pipefd[0]) == -1)
            {
                perror("close() error");
                exit(1);
            }

            exit(0);
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

int setnonblocking(int fd)
{
    int flags;
    if ((flags = fcntl(fd, F_GETFL,0)) == -1)
    {
        flags = 0;
    }
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}
