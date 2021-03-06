#include  <sys/socket.h>
#include  <time.h>
#include  <netinet/in.h>
#include  <arpa/inet.h>
#include  <errno.h>
#include  <fcntl.h>
#include  <stdio.h>
#include  <stdlib.h>
#include  <poll.h>
#include  <netdb.h>
#include  <strings.h>
#include  <Python.h>
#include  <signal.h>

#define MAXLINE 1024
#ifndef OPEN_MAX
#define OPEN_MAX 255
#endif
#define LISTENQ 1024
#define SERV_PORT 27

int keepRunning = 1;

void intHandler(int dummy) {
  keepRunning = 0;
}

void nowait(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int main(int argc, char **argv) {
  int                   i, maxi, listenfd, connfd, sockfd, j, k, nready, one=1, slen=0;
  ssize_t               n;
  socklen_t             clilen;
  struct pollfd         client[OPEN_MAX];
  struct sockaddr_in    cliaddr, servaddr;
  char                  *token, *cmd, line[MAXLINE];
  PyObject              *pName, *pModule, *func1, *func2, *func3, *func4, *pArgs, *pValue, *pFunc;

  fprintf(stderr, "%d", getpid());
  Py_Initialize();
  signal(SIGINT, intHandler);
  pName = PyString_FromString("network");
  pModule = PyImport_Import(pName);
  Py_DECREF(pName);
  if (pModule != NULL) {
    func1 = PyObject_GetAttrString(pModule, "func1");
    func2 = PyObject_GetAttrString(pModule, "func2");
    func3 = PyObject_GetAttrString(pModule, "func3");
    func4 = PyObject_GetAttrString(pModule, "func4");
    if (func1 && PyCallable_Check(func1) &&
        func2 && PyCallable_Check(func2) &&
        func3 && PyCallable_Check(func3) &&
        func4 && PyCallable_Check(func4)) {

            listenfd = socket(AF_INET, SOCK_STREAM, 0);
            setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
            nowait(listenfd);
            bzero(&servaddr, sizeof(servaddr));
            servaddr.sin_family      = AF_INET;
            servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
            servaddr.sin_port        = htons(SERV_PORT);

            bind(listenfd, (struct sockaddr*) &servaddr, sizeof(servaddr));

            listen(listenfd, LISTENQ);

            client[0].fd     = listenfd;
            client[0].events = POLLRDNORM;
            for (i = 1; i < OPEN_MAX; i++) client[i].fd = -1;
            maxi = 0;
            while (keepRunning) {
                nready = poll(client, maxi+1, -1);
                if (client[0].revents & POLLRDNORM) {
                    clilen = sizeof(cliaddr);
                    connfd = accept(listenfd, (struct sockaddr *) &cliaddr, &clilen);
                    for (i = 1; i < OPEN_MAX; i++)
                        if (client[i].fd < 0) {
                            nowait(connfd);
                            client[i].fd = connfd;
                            sendto(connfd, "Welcome to Pidometer: most powerfull tracker for your fitness, maths!\n", 70, 0, (struct sockaddr *) &cliaddr, sizeof(cliaddr));
                            break;
                        }
                    if (i == OPEN_MAX)
                        printf("too many clients\n");

                    client[i].events = POLLRDNORM;
                    if (i > maxi) maxi = i;
                    if (--nready <= 0) continue;
                }

                for (i = 1; i <= maxi; i++) {
                    if ((sockfd = client[i].fd) < 0) continue;
                    if (client[i].revents & (POLLRDNORM | POLLERR)) {
                        memset(line, 0, MAXLINE);
                        if ( (n = recvfrom(sockfd, line, MAXLINE, 0, (struct sockaddr *) &cliaddr, &clilen)) < 0) {
                            if (errno == ECONNRESET) {
                                close(sockfd);
                                client[i].fd = -1;
                            }
                        } else if (n == 0) {
                            close(sockfd);
                            client[i].fd = -1;
                        } else if (n > 1) {
                            if (strstr(line, "quit") != NULL) {
                                sendto(sockfd, "bye, bye!\n", 10, 0, (struct sockaddr *) &cliaddr, sizeof(cliaddr));
                                close(sockfd);
                                client[i].fd = -1;
                            } else {
                                cmd = strtok(line, " \r\n");
                                if (strcmp(cmd, "add") == 0) {
                                    pFunc = func1;
                                    n = 2;
                                } else if (strcmp(cmd, "register") == 0) {
                                    pFunc = func2;
                                    n = 1;
                                } else if (strcmp(cmd, "view")     == 0) {
                                    pFunc = func3;
                                    n = 3;
                                } else if (strcmp(cmd, "activity") == 0) {
                                    pFunc = func4;
                                    n = 1;
                                } else if (strcmp(cmd, "Question") == 0) {
                                    sendto(sockfd, "42\n", 3, 0, (struct sockaddr *) &cliaddr, sizeof(cliaddr));
                                    continue;
                                } else {
                                    sendto(sockfd, "Bad input\n", 10, 0, (struct sockaddr *) &cliaddr, sizeof(cliaddr));
                                    continue;
                                }
                                pArgs = PyTuple_New(n);
                                j = 0;
                                for (j = 0; j < n - 1; j++) {
                                    token = strtok(NULL, " \r\n");
                                    if (token) {
                                        pValue = PyString_FromString(token);
                                        if (pValue) {
                                            PyTuple_SetItem(pArgs, j, pValue);
                                            token = NULL;
                                            continue;
                                        }
                                    }
                                    break;
                                }
                                if (j == (n - 1)) {
                                    token = strtok(NULL, "\r\n");
                                    if (token) {
                                        pValue = PyString_FromString(token);
                                        if (pValue) {
                                            PyTuple_SetItem(pArgs, j++, pValue);
                                            token = NULL;
                                        }
                                    }
                                }
                                for (k = j; k < n; k++) {
                                    Py_INCREF(Py_None);
                                    PyTuple_SetItem(pArgs, k, Py_None);
                                }
                                pValue = PyObject_CallObject(pFunc, pArgs);
                                Py_DECREF(pArgs);
                                if (pValue) {
                                    if (PyString_Check(pValue)) {
                                           char *s = PyString_AsString(pValue);
                                           sendto(sockfd, s, strlen(s), 0, (struct sockaddr *) &cliaddr, sizeof(cliaddr));
                                    } else sendto(sockfd, "Error\n", 6, 0, (struct sockaddr *) &cliaddr, sizeof(cliaddr));
                                } else sendto(sockfd, "Error\n", 6, 0, (struct sockaddr *) &cliaddr, sizeof(cliaddr));
                                Py_DECREF(pValue);
                                PyErr_Clear();
                            }
                        }
                        if (--nready <= 0) break;
                    }
                }
            }
        }
    } else {
        PyErr_Print();
        return 1;
    }
    Py_Finalize();
    return 0;
}
