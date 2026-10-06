 #define _POSIX_C_SOURCE 200809L

#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <pthread.h>

#include <arpa/inet.h>

#include "../util/wrapper/Socket.h"
#include "../util/reactor_setup/cblib.h"
#include "tcp_gameserver.h"

#define BUFFER_SIZE (1 << 16)

int *fd_list[FD_SETSIZE];
state state_list[FD_SETSIZE];
state hb_state_list[FD_SETSIZE];
char *tcp_buffer_list[FD_SETSIZE];
heartbeat last_hb_list[FD_SETSIZE];
pinfo pinfo_list[FD_SETSIZE];

char errormessage[ERRORMESSAGE_SIZE];
static struct timer *Heartbeat_Timer;
//"Heartbeat" ist eine regelmäßige Signalübertragung

/**
 * Löscht einen Socket und befreit die Ressourcen für diesen Socket.
 * @param fd
 */
void clean_fd(int fd) {
    //printf("deleting fd: %d \n", fd);
    deregister_fd_callback(fd);
    if (fd_list[fd] != NULL)
    {
        free(fd_list[fd]);
        fd_list[fd] = NULL;
    }

    state_list[fd] = INIT;
    hb_state_list[fd] = INIT;
    memset(&(pinfo_list[fd]),0, sizeof(pinfo));
    if (tcp_buffer_list[fd])
    {
        free(tcp_buffer_list[fd]);
        tcp_buffer_list[fd] = NULL;
    }
    close(fd);
}

/**
 * Registriert einen neuen Socket und initialisiert die entsprechenden Datenstrukturen.
 * Prüft, ob die Anzahl der registrierten Sockets die maximale Größe überschreitet
 * @param fd
 */
void reg_fd(int *fd) {
    //printf("registering fd: %d \n", *fd);
    if(*fd >= FD_SETSIZE){
        perror("TOO MANY FDS");
        exit(-1);
    }
    fd_list[*fd] = fd;
    state_list[*fd] = WAITING_FOR_PLAYER_REQ;
    hb_state_list[*fd] = HB_READY;
    char* tmp = (char *) calloc(1, (size_t) sizeof(game_message));
    tcp_buffer_list[*fd]  = tmp;
}

/**
 * Überprüft, ob genügend Spieler registriert sind, um ein Spiel zu starten.
 * Wenn zwei Spieler gefunden werden, werden deren Informationen ausgetauscht
 */
void check_player_count() {
    int count = 0;
    int first_partner = 0;
    for (int i = 0; i < FD_SETSIZE; i++) {
        if (state_list[i] == WAITING_FOR_PLAYERS) {
            if (count == 0) {
                first_partner = i;
            }
            count++;
        }
        if (count == 2) {
            printf("Match found \n");
            peer_info pinfo1, pinfo2;
            //printf("pinfo1: %u %d %s %d \n",pinfo_list[first_partner].ip4_addr, pinfo_list[first_partner].port,
                 //  (char *) pinfo_list[first_partner].username,
                 //  pinfo_list[first_partner].username_len);
            pinfo1 = create_peer_info(pinfo_list[first_partner].ip4_addr, pinfo_list[first_partner].port, 1,
                                      (char *) pinfo_list[first_partner].username,
                                      pinfo_list[first_partner].username_len);
            pinfo2 = create_peer_info(pinfo_list[i].ip4_addr, pinfo_list[i].port, 0, (char *) pinfo_list[i].username,
                                      pinfo_list[i].username_len);

            game_message_container m1, m2;
            m1.peer_info = pinfo1;
            m2.peer_info = pinfo2;

            state_list[first_partner] = WAITING_FOR_PEER_INFO_ACK;
            state_list[i] = WAITING_FOR_PEER_INFO_ACK;

            send_game_message(*fd_list[i], m1.message);
            send_game_message(*fd_list[first_partner], m2.message);

            count = 0;
            first_partner = 0;
        }
    }
}

/**
 * Sendet periodisch Heartbeat-Nachrichten an alle registrierten Spieler.
 * Startet einen Timer für die erneute Übertragung von Heartbeat-Nachrichten
 * @param arg
 */
void heartbeat_timer(void *arg) {
    int *fd = 0;;
    printf("send heartbeat ...\n");
    for (int i = 0; i < FD_SETSIZE; i++) {
        if (hb_state_list[i] == HB_READY) {
            heartbeat hb;
            fd = fd_list[i];
            hb = create_hb("heartbeat");
            last_hb_list[i] = hb;
            send_game_message(*fd, *(game_message *) &hb);
            hb_state_list[i] = WAITING_FOR_HB_ACK;
            //start_timer(HB_Retransission_Timer, RETRANSMISSION_TIMER);
            start_timer(Heartbeat_Timer, HB_TIMER/10);
        }
    }
}
/**
 * Verarbeitet eingehende Heartbeat-Nachrichten und sendet eine Bestätigung zurück.
 * @param msg
 * @param fd
 * @return
 */

int handle_heartbeat(heartbeat msg, int fd) {
    //erstellt heartbeat_ack
    heartbeat_ack hb = create_hb_ack(msg);
    send_game_message(fd, *(game_message *) &hb);
    return 0;
}


/**
 *   Verarbeitet eingehende Bestätigungen für Heartbeat-Nachrichten.
 * @param msg
 * @param fd
 * @return
 */
int handle_heartbeat_ack(heartbeat_ack msg, int fd) {
    if (msg.hdr.length != ntohs(last_hb_list[fd].hdr.length)) {
        sprintf(errormessage, "HEARTBEAT LENGTHS NOT THE SAME: EXPECTED : %d BUT GOT %d",
                ntohs(last_hb_list[fd].hdr.length), msg.hdr.length);
        send_error_message(fd, errormessage);
    }

    if (strncmp((char *) msg.heartbeat, (char *) last_hb_list[fd].heartbeat, msg.hdr.length)) {
        sprintf(errormessage, "HEARTBEATS NOT THE SAME");
        send_error_message(fd, errormessage);
    }
    //stop_timer(HB_Retransission_Timer);
    hb_state_list[fd] = HB_READY;
    return 0;
}

/**
 *  Verarbeitet Anfragen zur Spielerregistrierung.
 *  Validiert die Anfrage und sendet eine Bestätigung zurück.
 *  Ändert den Status des Spielers auf "Warten auf Mitspieler" und prüft, ob genügend Spieler vorhanden sind, um ein Spiel zu starten.
 * @param msg
 * @param fd
 * @return
 */
int handle_registration_req(registration_req msg, int fd) {
    if(!validate_registration_req(msg)){
        sprintf(errormessage, "MALFORMED PLAYER_REQ");
        send_error_message(fd, errormessage);
    }
    if (state_list[fd] != WAITING_FOR_PLAYER_REQ) {
        sprintf(errormessage, "SERVER IS NOT WAITING FOR PLAYER_REQ");
        send_error_message(fd, errormessage);
    }

    long username_password_len = ntohs(msg.password_len) + ntohs(msg.username_len);
    uint16_t port = ntohs(msg.port);
    uint32_t ip4 = ntohl(msg.ip4_addr);
    uint32_t username_len =  ntohs(msg.username_len);
    if (username_password_len != msg.hdr.length-10) {
        sprintf(errormessage, "PASSWORD AND USERNAME  LENGTHS NOT RIGHT: EXPECTED : %ld BUT GOT %d",
                username_password_len, msg.hdr.length-10);
        send_error_message(fd, errormessage);
    }
    memcpy(pinfo_list[fd].username, msg.username_password, ntohs(msg.username_len));
    memcpy(pinfo_list[fd].password, msg.username_password + ntohs(msg.username_len), ntohs(msg.password_len));
    memcpy(&pinfo_list[fd].ip4_addr, &ip4, sizeof(ip4));
    memcpy(&pinfo_list[fd].port, &port, sizeof(port));
    memcpy(&pinfo_list[fd].username_len, &username_len, sizeof(username_len));
    //printf("Message : %u %d\n",ip4,port);
    //printf("pinfo set: %u %d %s %d \n",pinfo_list[fd].ip4_addr, pinfo_list[fd].port,
      //     (char *) pinfo_list[fd].username,
      //     pinfo_list[fd].username_len);

      printf("registered user : %s\n",pinfo_list[fd].username);
    registration_req_ack ack = create_registration_req_ack();
    game_message_container m;
    m.reg_ack = ack;
    send_game_message(fd, m.message);
    state_list[fd] = WAITING_FOR_PLAYERS;

    check_player_count();
    return 0;
}
/**
 * Verarbeitet Bestätigungen für Peer-Informationen und setzt den Status auf "Initialisiert" zurück.
 * @param msg
 * @param fd
 * @return
 */
int handle_peer_info_ack(peer_info_ack msg, int fd) {
    if (state_list[fd] != WAITING_FOR_PEER_INFO_ACK) {
        sprintf(errormessage, "SERVER WAS NOT EXPECTING PEER_INFO_ACK FROM THIS PEER");
        send_error_message(fd, errormessage);
    }
    state_list[fd] = INIT;
    return 0;
}


/**
 * Verarbeitet allgemeine Spielnachrichten und leitet sie an die entsprechenden Funktionen weiter, je nach Nachrichtentyp.
 * @param msg
 * @param fd
 */
void handle_game_message(game_message msg, int fd) {
    msg.hdr.length = ntohs(msg.hdr.length);
    msg.hdr.tag = ntohs(msg.hdr.tag);
    //printf("handling game_message with Tag: %d and Len: %d \n",msg.hdr.tag,msg.hdr.length);
    game_message_container msg_container;
    msg_container.message = msg;
    switch (msg.hdr.tag) {
        case SET_COLUM:
            sprintf(errormessage, "SERVER SHOULD NOT RECEIVE SET COLUM PACKAGE");
            send_error_message(fd, errormessage);
            break;
        case SET_COLUM_ACK:
            sprintf(errormessage, "SERVER SHOULD NOT RECEIVE SET_COLUM_ACK PACKAGE");
            send_error_message(fd, errormessage);
            break;
        case HEARTBEAT:
            handle_heartbeat(msg_container.hb, fd);
            break;
        case HEARTBEAT_ACK:
            handle_heartbeat_ack(msg_container.hb_ack, fd);
            break;
        case ERROR:
            handle_error(msg_container.error, fd);
            break;
        case REGISTRATION_REQ:
            handle_registration_req(msg_container.reg, fd);
            break;
        case REGISTRATION_REQ_ACK:
            sprintf(errormessage, "SERVER SHOULD NOT RECEIVE REGISTRATION_REQ_ACK PACKAGE");
            send_error_message(fd, errormessage);
            break;

        case PEER_INFO:
            sprintf(errormessage, "SERVER SHOULD NOT RECEIVE PEER_INFO PACKAGE");
            send_error_message(fd, errormessage);
            break;
        case PEER_INFO_ACK:
            handle_peer_info_ack(msg_container.peer_info_ack, fd);
            break;
        default:
            sprintf(errormessage, "NOT A RECOGNIZED MESSAGE TAG: %d", msg.hdr.tag);
            send_error_message(fd, errormessage);
    }
}

void receive_message(void *arg) {
    int fd = *(int *) arg, len;
    static int offset;
    char *tcp_buffer = tcp_buffer_list[fd];
    fd = *(int *) arg;
    len = Recv(fd, (void *) (tcp_buffer + offset), sizeof(game_message) - offset, 0);

   // printf("len: %d\n",len);
    // Orderly shutdown from peer
    if(len==0){
        printf("try to clean the file descriptor buffer \n");
        clean_fd(fd);
        return;
    }
    //NOTHING GOTTEN
    if (len < 0) {
            return;
    }
    offset += len;
    //printf("offset %d \n",offset);
    while (offset >= HEADER_SIZE) {
        message_hdr *hdr = (message_hdr *) tcp_buffer;
        int pad_size = calculate_pad_size(ntohs(hdr->length));
        int msg_len = ntohs(hdr->length) + pad_size + HEADER_SIZE;
        //Not a whole message
        if (msg_len > offset) {
            return;
        }
        handle_game_message(*(game_message *) tcp_buffer, fd);
        memmove(tcp_buffer, tcp_buffer + msg_len, offset - msg_len);
        offset -= msg_len;
    }
    return;
}


/**
 * Behandelt eingehende Verbindungsanfragen von neuen Spielern
 * @param args
 */
void handle_connections(void *args) {
    int fd = *(int *) args;
    struct sockaddr_in client_addr;
    socklen_t addr_len;
    int *cfd = (int *) malloc(sizeof(int));

    addr_len = (socklen_t)
    sizeof(client_addr);
    memset((void *) &client_addr, 0, sizeof(client_addr));
    *cfd = Accept(fd, (struct sockaddr *) &client_addr, &addr_len);

    if (fd_list[*cfd] != NULL) {
        clean_fd(*cfd);
    }
    reg_fd(cfd);
    register_fd_callback(*cfd, receive_message, (void *) cfd);
}

/**
 *  Bindet den Server an eine IP-Adresse und einen Port und gibt den Socket zurück.
 * @param fd
 * @param hostname
 * @param servname
 * @return
 */
int bind_server(int *fd, char *hostname, char *servname) {
    struct sockaddr_in addr;
    socklen_t len;

    struct addrinfo *srcres, hints;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
#ifdef HAVE_SIN_LEN
    addr.sin_len = sizeof(struct sockaddr_in);
#endif
    //addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // hier wird addr gesetzt
    addr.sin_port = htons(atoi(servname));    // port ändern nehme 9999
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    getaddrinfo(NULL, servname, &hints, &srcres);
    *fd = Socket(srcres->ai_family, srcres->ai_socktype, srcres->ai_protocol);

    int en = 1;
    setsockopt(*fd,SOL_SOCKET,SO_REUSEADDR,&en, sizeof(int));
    len = sizeof(addr);
    Bind(*fd, (const struct sockaddr *) &addr, (socklen_t) len);
    Listen(*fd,1);
    //Bind(*fd, srcres->ai_addr, srcres->ai_addrlen);
    return *fd;
}

int main(int argc, char **argv) {
    char *servname, *hostname;
    if (argc == 1) {
        hostname = "127.0.0.1";
        servname = "9999";
    } else if (argc == 3) {
        hostname = argv[1];
        servname = argv[2];
    } else {
        perror("wrong use of parameters \n");
        exit(-1);
    }
    init_cblib();
    int fd = 0;
    memset(fd_list, 0, sizeof(fd_list));
    memset(state_list, 0, sizeof(state_list));
    memset(hb_state_list, 0, sizeof(hb_state_list));

    Heartbeat_Timer = create_timer(heartbeat_timer, (void *) &fd, "heartbeat");
    start_timer(Heartbeat_Timer, HB_TIMER/10);
    bind_server(&fd, hostname, servname);
    register_fd_callback(fd, handle_connections, (void *) &fd);
    handle_events();
}


