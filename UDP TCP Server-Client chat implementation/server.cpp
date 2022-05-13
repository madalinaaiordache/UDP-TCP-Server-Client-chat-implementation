#include "helpers.h"

#define SERVER_IP "127.0.0.1"

using namespace std;

// Functie ce inchide toate socket-urile create
// Si trimite un mesaj clientilor TCP
// ce semnalizeaza ca serverul se va opri
void close_all_sockets(fd_set &fds, int fd_max, int listen_fd, int udp_fd) {
    for (int i = 0; i <= fd_max; ++i) {
        if (FD_ISSET(i, &fds) && i != STDIN_FILENO) {
			if (i != listen_fd && i != udp_fd) {
                udp_info_t info;
                // cod specific pentru server exit
                info.type = 4;

                int rs = send(i, &info, sizeof(info), 0);
                DIE(rs < 0, "Couldn't send to client\n");
            }

			close(i);
            FD_CLR(i, &fds);
        }
    } 
}

// pentru fiecare fd, retinem id-ul clientului cu care comunicam
map<int, string> fd_to_id;

//pentru fiecare id, tinem minte fd-ul socketului prin care comunicam
map<string, int> id_to_fd;


// colectie ce memoreaza mesajele primite de la clientii UDP,
// pentru a le putea trimite atunci cand are loc mecanismul de SF
map<string, vector<udp_info_t>> udp_messages;

// colectie ce asociaza fiecarui users o lista de abonari 
map<string, map<string, subscription_t>> subscriptions;

// colectie ce asocieaza fiecarui topic o lista de abonati
map<string, set<string>> subscribers;

// Trimite un mesaj tuturor clientilor TCP conectati la server
// abonati la topicul specific
void send_news_to_active_clients(const udp_info_t &info) {
    for (auto sub : subscribers[info.topic]) {
        // Verificam daca este activ clientul
        if (id_to_fd.find(sub) != id_to_fd.end()) {
            int subscriber_socket_fd = id_to_fd[sub];
            
            int rs = send(subscriber_socket_fd, &info, sizeof(info), 0);
            DIE(rs < 0, "Couldn't send to client\n");
        }
    }
}

// Functie ce trimite clientului TCP cu id-ul respectiv toate
// stirilie ce au fost publicate in timpul in care a fost inactiv
void send_missed_news(string id, int socket_fd) {
    // Prima oara se trimite numarul de topicuri unde 
    // s-au publicat mesaje in perioada de inactivitate
    int missed_topics = 0;

    for (auto sub : subscriptions[id]) {
        if (sub.second.last_receive + 1 < udp_messages[sub.first].size()
                    && sub.second.sf == 1) {
            ++missed_topics;                    
        }
    }
    
    int rs = send(socket_fd, &missed_topics, sizeof(missed_topics), 0);
    DIE(rs < 0, "Couldn't send to client\n");

    // Pentru fiecare topic "ratat", se va trimite mai intai numarul de 
    // mesaje ce au fost postate in acel topic, pentru ca subscriberul 
    // sa stie cate mesaje va primi (de cate ori trebuie sa faca recv() )
    for (auto sub : subscriptions[id]) {
        if (sub.second.last_receive + 1 < udp_messages[sub.first].size()
                    && sub.second.sf == 1) {

            int missed_messages = udp_messages[sub.first].size() -
                    sub.second.last_receive - 1;

            rs = send(socket_fd, &missed_messages, sizeof(missed_messages), 0);
            DIE(rs < 0, "Couldn't send to client\n");

            int start_pos = sub.second.last_receive + 1;

            // Se trimit toate mesajele pierdute
            for (int i = 0; i < missed_messages; ++i) {
                rs = send(socket_fd,
                    &(udp_messages[sub.first][start_pos + i]),
                    sizeof(udp_messages[sub.first][start_pos + i]),
                    0);
                DIE(rs < 0, "Couldn't send to client\n");
            }

            // ACtualizam care este ultimul mesaj primit de subscriber
            sub.second.last_receive = udp_messages[sub.first].size() - 1;
        }
    } 
}

int main(int argc, char **argv) {
    // Dezactivare buffering afisare
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    if (argc != 2) {
        cout << "Not enough paramaters for server\n"; 
    }  

    // Folosit pentru multiplexare
    fd_set sockets_fds, tmp_fds;
    int rs;
    bool server_up;

    // Cream socket pentru comunicarea UDP
    int udp_sock_fd = socket(PF_INET, SOCK_DGRAM, 0);
    DIE(udp_sock_fd == -1, "Can't create socket for UDP communication\n");

    struct sockaddr_in udp_sock_addr;
    int socket_len = sizeof(struct sockaddr_in);
    
    // Initializam adresa socket-ului
    memset(&udp_sock_addr, 0, socket_len);
    udp_sock_addr.sin_family = AF_INET;
    udp_sock_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    udp_sock_addr.sin_port = htons(atoi(argv[1]));

    // Asociem o adresa socketului
    rs = bind(udp_sock_fd, (struct sockaddr *) &udp_sock_addr, socket_len);
    DIE(rs == -1, "Can't bind socket\n");


    // Cream socket pentru comunicarea cu TCP (pe acest socket se va face listen)
    int listen_sock_fd = socket(PF_INET, SOCK_STREAM, 0);
    DIE(listen_sock_fd == -1, "Can't create socket for TCP communication\n");

    struct sockaddr_in listen_sock_addr;

    memset(&listen_sock_addr, 0, socket_len);
    listen_sock_addr.sin_family = AF_INET; // IPv4
    listen_sock_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    listen_sock_addr.sin_port = htons(atoi(argv[1]));

    // Asociem o adresa socketului
    rs = bind(listen_sock_fd, (struct sockaddr *) &listen_sock_addr, socket_len);
    DIE(rs == -1, "Can't bind socket\n");

    // Listen
    rs = listen(listen_sock_fd, MAX_LISTEN_QUEUE);
    DIE(rs < 0, "Can't listen\n");

    // Oprim Nagle
    deactivate_Nagle_algorithm(listen_sock_fd);

    // Initializam multimea de socketi
    FD_ZERO(&sockets_fds);
	FD_ZERO(&tmp_fds);
    int fd_max = -1;

    // Adaugam socket-ul pentru comunicare cu clientii UDP in multimea de socketi
    add_fd(sockets_fds, fd_max, udp_sock_fd);

    // Adaugam socket-ul pe care facem listen in multimea de socketi
    add_fd(sockets_fds, fd_max, listen_sock_fd);

    // Adaugam socket-ul lui STDIN in multimea de socketi
    add_fd(sockets_fds, fd_max, STDIN_FILENO);

    // Variabila ce reprezinta starea severului
    server_up = true;

    while(server_up) {
        tmp_fds = sockets_fds; 
		
        // Multiplexare
		rs = select(fd_max + 1, &tmp_fds, NULL, NULL, NULL);
		DIE(rs < 0, "select");

        for (int i = 0; i <= fd_max; ++i) {
            // Daca socketul nu este activ mergem mai departe
            if (!FD_ISSET(i, &tmp_fds)) {
                continue;
            }

            if (i == udp_sock_fd) {
                // se primesc date din partea clientilor udp
                udp_info_t info;
                sockaddr_in udp_recv_addr;
                memset(&info, 0, sizeof(udp_info_t));

                // Primim date intr-o structura de tip udp_info_t
                rs = recvfrom(udp_sock_fd, &info, sizeof(udp_info_t), 0,
                    (struct sockaddr *) &udp_recv_addr, (socklen_t *) &socket_len);
                DIE(rs == -1, "Can't recv\n");

                // Punem in structura info IP-ul si portul 
                // prin care a avut loc comunicarea cu clientul udp
                memcpy(info.IP, inet_ntoa(udp_recv_addr.sin_addr), sizeof(info.IP));
                info.port = udp_recv_addr.sin_port;

                // Adaugam acest mesaj in multimea de mesaje
                udp_messages[info.topic].push_back(info);

                // Trimitem mesajul tuturor clientilor care sunt conectati la server
                send_news_to_active_clients(info);
            }
            else if (i == listen_sock_fd) {
                // a venit o cerere de conexiune pe socketul inactiv
                // (cel cu listen), pe care serverul o accepta
                struct sockaddr_in client_addr;
                int len = sizeof(client_addr);
                int client_sock_fd = accept(listen_sock_fd,
                        (struct sockaddr *)&client_addr, (socklen_t *) &len);
                DIE(client_sock_fd == -1, "Can't accept\n");

                // Serverul primeste id-ul clientului
                char client_id[MAX_CLIENT_ID_SIZE + 1];
                rs = recv(client_sock_fd, client_id, sizeof(client_id), 0);
                DIE(rs < 0, "Couldn't receive from client");

                string id_string = string(client_id);

                // Se verifica daca ID-ul primit este valid 
                // si se trimite inapoi clientului TCP un raspuns 
                // ce reprezinta daca a avut loc cu succes conexiunea
                bool valid_client = true;

                // Daca exista deja un client cu acelasi id
                if (id_to_fd.find(id_string) != id_to_fd.end()) {
                    valid_client = false;
                }

                rs = send(client_sock_fd, &valid_client, sizeof(valid_client), 0);
                DIE(rs < 0, "Couldn't send to client\n");

                if (!valid_client) {
                    cout << "Client " << id_string << " already connected.\n";
                    continue;
                }

                // Adaugam clientul in baza de date. Facem asocierea ID <=> FD
                id_to_fd[id_string] = client_sock_fd;
                fd_to_id[client_sock_fd] = id_string;

                // se adauga noul socket intors de accept() la multimea
                // descriptorilor de citire
                add_fd(sockets_fds, fd_max, client_sock_fd);

                cout << "New client " << id_string << " connected from " <<
                        inet_ntoa(client_addr.sin_addr) << ":" <<
                        ntohs(client_addr.sin_port) << ".\n";

                // Trimitem toate mesajele pe care clientul le-a ratat
                // cand a fost inactiv (daca este cazul)
                send_missed_news(id_string, client_sock_fd);
            } else if (i == STDIN_FILENO) {
                // serverul primeste o comanda
                string command;
                cin >> command;
                if (command == "exit") {
                    // se opreste serverul
                    server_up = false;
                    break;
                }
            } else {
                // serverul primeste actiuni din partea clientilor tcp
                client_action_t msg;
                recv(i, &msg, sizeof(client_action_t), 0);
                DIE(rs < 0, "Couldn't receive from client");

                string client_id;
                subscription_t subscr;
    
                switch (msg.action)
                {
                    case 0:
                    // exit
                        // Se sterge din multimea de FD
                        FD_CLR(i, &sockets_fds);
                        close(i);

                        cout << "Client " << fd_to_id[i] << " disconnected.\n";

                        client_id = fd_to_id[i];

                        // Se sterge asocierea ID <=> FD (pentru ca e posibil
                        // ca la urmatoarea conectare clientul sa aibe un alt id)
                        id_to_fd.erase(client_id);
                        fd_to_id.erase(i);

                        // Actualizam pentru subscriptiile SF, care este ultimul
                        // mesaj primit inainte de deconectare
                        for (auto &it : subscriptions) {
                            for (auto &sub : it.second) {
                                if (sub.second.sf == 1) {
                                    sub.second.last_receive =
                                        udp_messages[sub.first].size() - 1;
                                }
                            }
                        }

                        break;
                    case 1:
                    // subscribe
                        subscr.sf = msg.sf;

                        client_id = fd_to_id[i];
                        subscriptions[client_id][msg.topic] = subscr;
                        subscribers[msg.topic].insert(client_id);
                        break;
                    case 2:
                    // unsubscribe
                        client_id = fd_to_id[i];
                        subscriptions[client_id].erase(msg.topic);
                        subscribers[msg.topic].erase(client_id);
                        break;
                    default:
                        break;
                }
            }
        }
    }

    // Inchidem toti socketii si inchidem si toti clentii TCP
    close_all_sockets(sockets_fds, fd_max, listen_sock_fd, udp_sock_fd);

    return 0;
}