#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdbool.h>
#include <pthread.h>

#define PORT 7000

typedef int SOCKET;

int8_t yes = 1;
fd_set curr_fds_set, main_fds_set, tmp_fds_set;
struct sockaddr_in server, client;
uint32_t l = sizeof(struct sockaddr_in), bs, br;
uint16_t client_count = 0;
uint16_t new_port, c_port;

int main(int argc, char** argv) {

    SOCKET srv_tcp, max_fd, curr_fd, new_fd, fd;
    char buf[256], msg[200];

    srv_tcp = socket(AF_INET, SOCK_STREAM, 0);

    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);

    if (setsockopt(srv_tcp, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) < 0) {
        perror("setsockopt error:");
        exit(1);
    }

    if (bind(srv_tcp, (const struct sockaddr*)&server, sizeof(server)) < 0) {

        perror("Bind error");
        return 1;
    }

    FD_ZERO(&curr_fds_set);
    FD_ZERO(&main_fds_set);
    FD_SET(srv_tcp, &main_fds_set);
    max_fd = srv_tcp;

    listen(srv_tcp, 16);

    while (true) {

        curr_fds_set = main_fds_set;

        if (select(max_fd+1, &curr_fds_set, NULL, NULL, NULL) < 0) {

            perror("Error on select");
            return 1;
        }

        for (curr_fd = 3; curr_fd <= max_fd; curr_fd++) {

            if (FD_ISSET(curr_fd, &curr_fds_set)) {

                if (curr_fd == srv_tcp) {

                    // Treating when a new client has arrived.
                    memset(&client, 0, sizeof(client));

                    new_fd = accept(srv_tcp, (struct sockaddr*)&client, &l);
                    new_port = client.sin_port;

                    if (new_fd < 0) {

                        perror("Error on accept");
                        close(srv_tcp);
                        return 1;
                    }

                    if (new_fd > max_fd) {
                        max_fd = new_fd;
                    }

                    FD_SET(new_fd, &main_fds_set);

                    printf("New client connected from %s:%d ; socket #%d\n", inet_ntoa(client.sin_addr), ntohs(client.sin_port), new_fd);
                    client_count++;

                    client_count = htons(client_count);
                    send(new_fd, &client_count, sizeof(uint16_t), 0);
                    client_count = ntohs(client_count);

                    memset(buf, 0, 256);
                    sprintf(buf, "[PRIVATE] You have successfully connected to the server.\n          You are client with ID #%d, there are %d clients currently.\n", new_fd, client_count);

                    send(new_fd, buf, sizeof(buf), 0);

                    tmp_fds_set = main_fds_set;

                    for (fd = 3; fd <= max_fd; fd++) {

                        if (FD_ISSET(fd, &tmp_fds_set) && fd != new_fd && fd != srv_tcp) {

                            getpeername(fd, (struct sockaddr*)&client, &l);
                            strcpy(buf, inet_ntoa(client.sin_addr));
                            c_port = client.sin_port;

                            send(new_fd, buf, 256, 0);
                            send(new_fd, &c_port, sizeof(uint16_t), 0);
                        }
                    }
                    memset(buf, 0, 256);

                    tmp_fds_set = main_fds_set;
                    // Sending the arrival notification to all clients except for the new one via tcp.
                    for (fd = 3; fd <= max_fd; fd++) {

                        if (FD_ISSET(fd, &tmp_fds_set) && fd != srv_tcp && fd != new_fd) {

                            strcpy(buf, "+");
                            strcat(buf, inet_ntoa(client.sin_addr));

                            send(fd, buf, 256, 0);
                            send(fd, &new_port, sizeof(uint16_t), 0);

                        }
                    }
                }

                else {

                    memset(msg, 0, 200);
                    memset(buf, 0, 256);

                    br = recv(curr_fd, msg, sizeof(msg), 0);

                    getpeername(curr_fd, (struct sockaddr*)&client, &l);

                    if (strcmp(msg, "QUIT") == 0 || br == 0) {

                        if (br == 0) {

                            printf("Client on socket #%d forcibly hung up.\n", curr_fd);
                        }
                        else {

                            printf("Received disconnection request from: %s:%d ; socket #%d.\n", inet_ntoa(client.sin_addr), ntohs(client.sin_port), curr_fd);

                            sprintf(buf, "Disconnection request granted.\n");
                            send(curr_fd, buf, sizeof(buf), 0);
                        }

                        memset(buf, 0, 256);

                        tmp_fds_set = main_fds_set;

                        for (fd = 3; fd <= max_fd; fd++) {

                            if (FD_ISSET(fd, &tmp_fds_set) && fd != srv_tcp && fd != curr_fd) {

                                strcpy(buf, "-");
                                strcat(buf, inet_ntoa(client.sin_addr));
                                new_port = client.sin_port;

                                send(fd, buf, 256, 0);
                                send(fd, &new_port, sizeof(uint16_t), 0);
                            }
                        }

                        close(curr_fd);
                        FD_CLR(curr_fd, &main_fds_set);
                        client_count--;
                    }
                    else {

                        printf("Message sent by %s:%d - \"%s\"\n", inet_ntoa(client.sin_addr), ntohs(client.sin_port), msg);

                    }

                }
            }
        }
    }
    return 0;
}