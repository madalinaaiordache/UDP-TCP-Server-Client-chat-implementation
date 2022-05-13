#include "helpers.h"

using namespace std;

// Afiseaza tipul datelor primite
void print_type(int type) {
    switch (type)
    {
    case 0:
        cout << "INT";
        break;
    case 1:
        cout << "SHORT_REAL";
        break;
    case 2:
        cout << "FLOAT";
        break;
    case 3:
        cout << "STRING";
        break;
    default:
        break;
    }
}

// Formateaza si afiseaza payload-ul
void print_content(int type, char* content) {
    uint32_t decimal;
    int sol, power;
    uint16_t short_decimal;
    int sign;
    float short_sol;
    double real;

    switch (type)
    {
    case 0:
        // Byte-ul de semn
        sign = (int)content[0];

        if (sign != 0) {
            cout << "-";
        }

        ++content;
        decimal = *((uint32_t *)content);
        
        cout << ntohl((int)decimal);
        break;
    case 1:
        short_decimal = (ntohs(*((uint16_t *)(content))));

        short_sol = 1.0 * short_decimal / 100;

        cout << fixed << setprecision(2) << (float)short_sol;

        break;
    case 2:
        sign = (int)content[0];

        // Byte-ul de semn
        if (sign != 0) {
            cout << "-";
        }

        ++content;
        // Obtinem cifrele numarului
        decimal = *((uint32_t *)content);
        decimal = ntohl((int)decimal);

        power = (int) content[4];
        real = 1.0 * decimal * pow(10, -power);

        cout << fixed << setprecision(4) << real;
        break;
    case 3:
        cout << string(content);
        break;
    default:
        break;
    }
}

// Afiseaza un mesaj primit de la server
void print_message(udp_info_t info) {
    cout << string(info.IP) << ":" << info.port << " - " <<
     info.topic << " - ";
    print_type(info.type);
    cout << " - ";
    print_content(info.type, info.content);
    cout << "\n";   
}

// Funcite ce primeste toate mesajele trimise in perioada
// de inactivitate pentru subscriptii SF = 1
void receive_sf_messages(int server_sock_fd) {
    int rs, missed_topics, missed_messages;

    // Se primeste numarul de topicuri unde s-au postat mesaje
    rs = recv(server_sock_fd, &missed_topics, sizeof(missed_topics), 0);
    DIE(rs < 0, "Unable to recv\n");

    for (int i = 0; i < missed_topics; ++i) {
        // Pentru fiecare topic, se primeste numarul de mesaje ratate
        rs = recv(server_sock_fd, &missed_messages, sizeof(missed_messages), 0);
        DIE(rs < 0, "Unable to recv\n");

        for (int j = 0; j < missed_messages; ++j) {
            udp_info_t info;
            // Se primesc mesajele ratate
            rs = recv(server_sock_fd, &info, sizeof(info), 0);
            DIE(rs < 0, "Unable to recv\n");

            // Se afiseaza mesajul primit
            print_message(info);
        }
    }
}

int main(int argc, char **argv) {
    // Dezactivare buffering afisare
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    if (argc != 4) {
        cout << "Not enough paramaters for server\n"; 
    }  

    // Folosit pentru multiplexare
    fd_set sockets_fds, tmp_fds;
    int rs;
    bool client_up;
    char buff[MAX_COMMAND_SIZE];

    // Initializam multimea de socketi
    FD_ZERO(&sockets_fds);
    int fd_max = -1;

    // Cream socket pentru comunicarea cu TCP
    int server_sock_fd = socket(PF_INET, SOCK_STREAM, 0);
    DIE(server_sock_fd == -1, "Can't create socket for TCP communication\n");

    struct sockaddr_in sock_addr;
    int socket_len = sizeof(struct sockaddr_in);

    memset(&sock_addr, 0, socket_len);
    sock_addr.sin_family = AF_INET; // IPv4
    sock_addr.sin_addr.s_addr = inet_addr(argv[2]);
    sock_addr.sin_port = htons(atoi(argv[3]));

    // Conectam socketul la server
    rs = connect(server_sock_fd, (struct sockaddr *) &sock_addr, socket_len);
    DIE(rs < 0, "Unable to connect\n");

    // Oprim Nagle
    deactivate_Nagle_algorithm(server_sock_fd);

    char *client_id = argv[1];

    // Trimitem server-ului id ul clientului
    rs = send(server_sock_fd, client_id, strlen(client_id) + 1, 0);
    DIE(rs < 0, "Unable to send\n");

    // Verificam raspunsul serverului
    bool valid_client;
    rs = recv(server_sock_fd, &valid_client, sizeof(valid_client), 0);
    DIE(rs < 0, "Unable to recv\n");

    if (!valid_client) {
        // Clientul este invalid, deci nu paote opera
        close(server_sock_fd);
        return 0;
    }

    // Primim toate mesajele care au fost postate in topicuri
    // unde subscriptia era de tipul SF = 1
    receive_sf_messages(server_sock_fd);

    // Adaugam socket-ul serverului in multimea de socketi
    add_fd(sockets_fds, fd_max, server_sock_fd);

    // Adaugam socket-ul lui STDIN in multimea de socketi
    add_fd(sockets_fds, fd_max, STDIN_FILENO);
    
    // Variabila ce reprezinta starea clientului
    client_up = true;

    while(client_up) {
        tmp_fds = sockets_fds; 
		
        // Multiplexare
		rs = select(fd_max + 1, &tmp_fds, NULL, NULL, NULL);
		DIE(rs < 0, "select");

        for (int i = 0; i <= fd_max; ++i) {
            // Daca socketul nu este activ mergem mai departe
            if (!FD_ISSET(i, &tmp_fds)) {
                continue;
            }

            if (i == server_sock_fd) {
                // Se primesc date de la server
                udp_info_t info;

                rs = recv(server_sock_fd, &info, sizeof(info), 0);
                DIE(rs < 0, "Unable to recv\n"); 

                if (info.type == 4) {
                    // Serverul se va inchide
                    client_up = false;
                    break;
                }

                // Se afiseaza mesajul primit
                print_message(info);
                
            } else if (i == STDIN_FILENO) {
                // Clientul primeste o comanda de la STDIN
                if (!fgets(buff, MAX_COMMAND_SIZE, stdin)) {
                    fprintf(stderr, "Invalid command.\n");
                    continue;
                }

                client_action_t msg;
                istringstream iss(buff);
                string action;

                if (!(iss >> action)) {
                    fprintf(stderr, "Invalid command.\n");
                    continue;
                }

                if (action == "exit") {
                    msg.action = 0;
                    
                    // Se trimite catre server actiunea "exit client"
                    rs = send(server_sock_fd, &msg, sizeof(msg), 0);
                    DIE(rs < 0, "Unable to send\n");

                    client_up = false;
                    break;
                } else if (action == "subscribe") {
                    msg.action = 1;
                    
                    if (!(iss >> msg.topic >> msg.sf)) {
                        fprintf(stderr, "Invalid command.\n");
                        continue;
                    }

                    // Se trimite catre server actiunea "subscribe client"
                    rs = send(server_sock_fd, &msg, sizeof(msg), 0);
                    DIE(rs < 0, "Unable to send\n");

                    cout << "Subscribed to topic.\n";
                } else if (action == "unsubscribe") {
                    msg.action = 2;

                    if (!(iss >> msg.topic)) {
                        fprintf(stderr, "Invalid command.\n");
                        continue;
                    }

                    // Se trimite catre server actiunea "unsubscribe client"
                    rs = send(server_sock_fd, &msg, sizeof(msg), 0);
                    DIE(rs < 0, "Unable to send\n");

                    cout << "Unsubscribed to topic.\n";
                } else {
                    fprintf(stderr, "Invalid command.\n");
                }
            }
        }
    }

    // Se inchide socket-ul pentru comunicarea cu serverul
    close(server_sock_fd);
    return 0;
}