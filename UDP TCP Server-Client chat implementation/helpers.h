#include <bits/stdc++.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <features.h>

#define DIE(assertion, call_description) \
	do {								\
		if (assertion) {					\
			fprintf(stderr, "(%s, %d): ",			\
					__FILE__, __LINE__);		\
			perror(call_description);			\
			exit(EXIT_FAILURE);				\
		}							\
	}  while(0)  \


// Functie ce adauga un nou file descriptor intr-un fd_set
void add_fd(fd_set &fds, int &fd_max, int fd) {
    FD_SET(fd, &fds);
	if (fd > fd_max) {
		fd_max = fd;
	}
}

// Functie ce dezactiveaza algoritmul Nagle pentru comunicarea TCP
void deactivate_Nagle_algorithm(int socket) {
	int val = 1;
	int rs = setsockopt(socket, IPPROTO_TCP, 1, &val, sizeof(val));
	DIE(rs == -1, "Can't stop Neagle");
}

#define MAX_LISTEN_QUEUE 10
#define MAX_CLIENT_ID_SIZE 10
#define MAX_TOPIC_SIZE 50
#define MAX_COMMAND_SIZE 100
#define MAX_CONTENT_SIZE 1500
#define MAX_IP_SIZE 15

// Structura folosita pentru retinerea unei subscriptii
struct subscription_t {
    int sf = 0;
    int last_receive = 0;
};

// Structura folosita pentru transmiterea datelor de la
// clientii TCP catre server
struct client_action_t {
	int action; // 0 - exit, 1 - subscribe, 2 - unsubscribe
	char topic[MAX_TOPIC_SIZE + 1];
	int sf;
};

// Structura folosita pentru transmiterea datelor de la 
// server catre clientii TCP (date ce provin de la clientii UDP)
struct udp_info_t {
	char topic[MAX_TOPIC_SIZE];
	char type; /* 0 - INT
				  1 - SHORT_REAL
				  2 - FLOAT
				  3 - STRING
				  4 - EXIT (serverul urmeaza sa fie inchis si transmite
				  		acest lucru tuturor clientilor TCP) */
	char content[MAX_CONTENT_SIZE];
	char IP[MAX_IP_SIZE + 1];
	int port;
};
