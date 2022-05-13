IORDACHE Madalina Gabriela 323CA



Server:
    Folosesc urmatoarele hashtable-uri (std::map) :
    - fd_to_id si id_to_fd: constuiesc asocierea dintre id-ul unui client
    si fd-ul socket-ului prin care se face comunicarea cu acel client.
    De fiecare data cand un client se deconecteaza, sterg asocierea din 
    ambele map-uri.

    - udp_messages: memoreaza mesajele primite de la clientii UDP, pentru
    a putea trimite mai tarziu mesajele clientilor ce au fost inactivi, 
    dar au avut abonamente de tipul SF.

    - subscriptions: pentru fiecare user, tin minte care sunt abonamentele
    acestuia (care este topicul la care este abonat, daca abonamentul este
    de tip SD si care a fost ultimul mesaj primit din acel topic);

    - subscribers: pentru fiecare topic, tin minte lista de utilizatori
    abonati la acel topic

    Pornesc initial 2 socket-uri, unul pentru comunicarea cu UDP si unul 
    pe care se va face listen (aici se vor conecta lientii TCP).
    Realizez multiplexare folosind FD_SET si select :
        1. Se comunica pe socket-ul UDP. Prmesc informatia, construiesc pachetul
    pe care urmeaza sa il trimit abonatilor (un struct udp_info_t), dupa care
    il trimit tuturor clientilor TCP activi.
        2. Se comunica pe socket-ul de listen. Asta inseamna ca un nou client TCP
    doreste sa se conecteze. Creez un socket nou care va fi folosit pentru 
    comunicarea cu acest client. Dupa aceea verific daca ID-ul este unul valid
    si transmit inapoi clientului daca se poate conecta cu succes la server.
    Trimit toate mesajele ce au fost ratate de catre client din abonarile cu
    SF (daca este cazul).
        3. Se da o comanda de la STDIN pentru server. Aceasta poate fi doar exit,
    iar serverul se opeste.
        4. Se primesc date pe unul dintre socketii creati pentru comunicarea cu
    clientii TCP. Asta inseamna ca un client vrea sa ne transmita o actiune 
    (Subscribe, Unsubscribe, Exit). In cazul de exit, sterg clientul din baza 
    de date (fd_to_id si id_to_fd) si din FD_SET.

    La final, am grija sa transmit tuturor clientilor TCP activi ca serverul urmeaza
    sa se inchida, astfel vor trebui si ei sa se opreasca. De asemenea, inchid toti
    socketii care au fost deschisi.

Subscriber:
    Creez un socket care va comunica prin TCP cu serverul.

    Astept raspunsul serverului care imi spune daca ma pot conecta sau nu.
    Primesc toate mesajele pe care le-am ratat din topicurile cu SF (daca este cazul).
    Realizez multiplexare folosind FD_SET si select :
        1. Primesc date de la server. 
            - serverul se va inchide, asa ca trebuie si clientul sa se opreasca.
            - a fost transmis un mesaj de la server (ce provine din
                partea clientului UDP), asa ca il voi afisa.
        2. Se citeste o comanda de la tastatura. Am grija ca nu cumva comanda
        sa fie invalida, dupa care imi pregatesc pachetul pe care il voi trimite 
        serverului (struct client_action_t) specificand ce actiune urmeaza sa 
        faca clientul (Subscribe, Unsubscribe, Exit).

    La final inchid socketul pentru comunicarea cu serverul.
