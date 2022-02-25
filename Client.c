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

fd_set curr_rfds_set, main_rfds_set;
int32_t bs, br;
uint32_t l = sizeof(struct sockaddr_in);
uint16_t target_count, index1, index2;
struct sockaddr_in targets[16];
char signal_type;
int yes = 1;

int main(int argc, char** argv) {

    SOCKET srv_clnt_tcp, clnt_target_udp, recv_udp, max_fd;
    struct sockaddr_in server, from, target, self_addr;
    struct hostent *hp;
    char buf[256], msg[200];
    uint16_t new_port;

    if (argc != 2) {

        printf("Usage: %s <server_name>\n", argv[0]);
        return 1;
    }

    srv_clnt_tcp = socket(AF_INET, SOCK_STREAM, 0);

    server.sin_addr.s_addr = inet_addr(argv[1]);
    server.sin_port = htons(PORT);
    server.sin_family = AF_INET;

    if (connect(srv_clnt_tcp, (const struct sockaddr*)&server, sizeof(server)) < 0) {

        perror("Unable to connect to server");
        return 1;
    }

    recv(srv_clnt_tcp, &target_count, sizeof(uint16_t), 0);

    target_count = ntohs(target_count)-1;

    memset(buf, 0, 256);
    br = recv(srv_clnt_tcp, buf, 256, 0);

    printf("%s\n", buf);

    memset(buf, 0, 256);

    for (index1 = 0; index1 < target_count; index1++) {

        recv(srv_clnt_tcp, buf, 256, 0);
        recv(srv_clnt_tcp, &new_port, sizeof(uint16_t), 0);

        hp = gethostbyname(buf);
        memmove(&targets[index1].sin_addr, hp->h_addr, hp->h_length);

        targets[index1].sin_family = AF_INET;
        targets[index1].sin_port = new_port;
    }

    clnt_target_udp = socket(AF_INET, SOCK_DGRAM, 0);

    recv_udp = socket(AF_INET, SOCK_DGRAM, 0);

    self_addr.sin_addr.s_addr = INADDR_ANY;
    self_addr.sin_port = htons(PORT+1);
    self_addr.sin_family = AF_INET;

    if (setsockopt(clnt_target_udp, SOL_SOCKET, SO_BROADCAST, &yes,sizeof(int)) < 0) {
        perror("setsockopt error");
        return 1;
    }

    if (setsockopt(recv_udp, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) < 0) {
        perror("setsockopt error");
        return 1;
    }

    if (bind(recv_udp, (struct sockaddr*)&self_addr, l) < 0) {
        perror("Bind error");
        return 1;
    }

    FD_ZERO(&curr_rfds_set);
    FD_ZERO(&main_rfds_set);
    FD_ZERO(&curr_rfds_set);
    FD_SET(0, &main_rfds_set);
    FD_SET(srv_clnt_tcp, &main_rfds_set);
    FD_SET(recv_udp, &main_rfds_set);
    max_fd = (srv_clnt_tcp < recv_udp) ? recv_udp : srv_clnt_tcp;

    while (true) {

        curr_rfds_set = main_rfds_set;

        if (select(max_fd+1, &curr_rfds_set, NULL, NULL, NULL) < 0) {

            perror("Error on select");
            return 1;
        }


        if (FD_ISSET(0, &curr_rfds_set)) {

            // Accepting messages from the client's input.

            memset(buf, 0, 256);
            memset(msg, 0, 200);

            br = read(0, msg, sizeof(msg)-1);
            msg[br-1] = 0;

            if (strcmp(msg, "QUIT") == 0) {

                // Treating the case where the client disconnects.
                bs = send(srv_clnt_tcp, msg, 200, 0);

                br = recv(srv_clnt_tcp, buf, 256, 0);

                printf("[SERVER] %s\n", buf);

                close(srv_clnt_tcp);
                return 1;
            }
            else {

                // Treating the case where the client sends a message to all clients.

                target.sin_addr.s_addr = inet_addr("192.168.1.255");
                target.sin_port = htons(PORT+1);
                target.sin_family = AF_INET;

                bs = sendto(clnt_target_udp, msg, 200, 0, (struct sockaddr*)&target, l);

                bs = send(srv_clnt_tcp, msg, 200, 0);
            }
        }

        if (FD_ISSET(srv_clnt_tcp, &curr_rfds_set)) {

            // Accepting arrivals, departures or connection confirmation from the server via tcp
            // and connect to the new clients using the notification data via udp.

            memset(buf, 0, 256);
            br = recv(srv_clnt_tcp, buf, 256, 0);

            if (buf[0] == '+' || buf[0] == '-') {

                signal_type = buf[0];

                strcpy(buf, buf+1);
                br = recv(srv_clnt_tcp, &new_port, sizeof(uint16_t), 0);

                memset(&target, 0, l);
                target.sin_family = AF_INET;

                hp = gethostbyname(buf);

                if (!hp) {

                    perror("Invalid host");
                    return 1;
                }

                memmove(&target.sin_addr, hp->h_addr, hp->h_length);
                target.sin_port = new_port;

                if (signal_type == '+') {

                    printf("[SERVER] Client from %s:%d has joined the chat.\n", inet_ntoa(target.sin_addr), ntohs(target.sin_port));
                    targets[target_count] = target;
                    target_count++;
                }
                else {

                    printf("[SERVER] Client from %s:%d has left the chat.\n", inet_ntoa(target.sin_addr), ntohs(target.sin_port));
                    for (index1 = 0; index1 < target_count; index1++) {

                        if (targets[index1].sin_addr.s_addr == target.sin_addr.s_addr && targets[index1].sin_port == target.sin_port) {

                            for (index2 = index1; index2 < target_count-1; index2++) {

                                targets[index2] = targets[index2+1];
                            }

                            memset(&targets[target_count-1], 0, l);
                            target_count--;
                            break;
                        }
                    }
                }
            }
        }
        if (FD_ISSET(recv_udp, &curr_rfds_set)) {

            // Accepting messages from other clients via udp.
            memset(&from, 0, l);
            memset(msg, 0, 200);
            br = recvfrom(recv_udp, msg, 200, 0, (struct sockaddr*)&from, &l);

            printf("Received message from %s:%d - %s\n", inet_ntoa(from.sin_addr), ntohs(from.sin_port), msg);
        }


    }
    return 0;
}