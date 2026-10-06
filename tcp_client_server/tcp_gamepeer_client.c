/*-
 * Copyright (c) 2013 Michael Tuexen
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
 
 #define _POSIX_C_SOURCE 200809L


#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "../util/game_setup/4clib.h"
#include "../util/wrapper/Socket.h"
#include "../util/reactor_setup/cblib.h"

#include "../udp_peer_2_peer/gamepeer.h"
#include "../util/protocol/gameprotocoll.h"

#define BUFFER_SIZE (1 << 16)
static struct timer *Heartbeat_Timer, *Retransmission_Timer, *HB_Retransission_Timer;
int this_player, opponent_player, next_seq, next_seq_opp;
state current_game_state;
state hb_state;
char errormessage[ERRORMESSAGE_SIZE];

char *tcp_buffer;

uint16_t last_move = 0;
heartbeat last_heartbeat;
game_message last_message;

unsigned int connected = 0;
int bound_fd=0;

char* bound_ip ;//= "127.0.0.1";
//char* bound_ip = "192.168.1.120";
uint16_t bound_port = 9999;

int retransmission_count = 0;
int hb_retransmission_count = 0;

void handle_winner() {
    if (winner() == this_player) {
        printf("YOU WON!!! \n");
    } else {
        printf("OPPONENT WON!!!\n");
    }
    exit(0);
}

void make_4win_move(int colum, int player) {
    if (!valid_move(colum)) {
        perror("Something went wong that is not a valid move");
        return;
    }

    make_move(colum, player);
    print_board();



}

void this_starts() {
    opponent_player = PLAYER_2;
    this_player = PLAYER_1;
    current_game_state = PLAYER_TURN;
    printf("you start \n");
}

void opponent_starts() {
    opponent_player = PLAYER_1;
    this_player = PLAYER_2;
    current_game_state = OPPONENT_TURN;
    printf("opponent starts \n");
}

void turn_order(uint16_t startflag) {
    if (startflag) {
        this_starts();
    } else {
        opponent_starts();
    }
    printf("This player: %d \n", this_player);
}

void reset_retransmission(int * retransmission_count, struct timer** timer){
    *retransmission_count=0;
    stop_timer(*timer);
}

int calculate_retransmission_time(int retransmission_count){
    if(retransmission_count >RETRANSMISSION_MAX){
        perror("too many retransmissions");
        exit(-1);
    }
    int res = RETRANSMISSION_TIMER;
    //int res = RETRANSMISSION_TIMER * (1 << retransmission_count);
    for(int i= 0; i<retransmission_count;i++){
        res=res*2;
    }
    return res;
}
void retransmission_timer(void *arg) {
    int fd;
    fd = *(int *) arg;
    retransmission_count++;
    printf("retransmission of message with Tag %hu\n", ntohs(last_message.hdr.tag));
    send_game_message(fd, last_message);
    start_timer(Retransmission_Timer, calculate_retransmission_time(retransmission_count));
}

void heartbeat_retransmission_timer(void *arg) {
    int fd;
    fd = *(int *) arg;
    hb_retransmission_count++;
    printf("retransmission of heartbeat: %s \n", last_heartbeat.heartbeat);
    game_message_container msg;
    msg.hb = last_heartbeat;
    send_game_message(fd, msg.message);
    start_timer(HB_Retransission_Timer, calculate_retransmission_time(hb_retransmission_count));
}

void heartbeat_timer(void *arg) {
    int fd;

    fd = *(int *) arg;
    if (hb_state == HB_READY) {
        heartbeat hb;
        hb = create_hb("heartbeat");
        last_heartbeat = hb;
        send_game_message(fd, *(game_message *) &hb);
        hb_state = WAITING_FOR_HB_ACK;
        start_timer(HB_Retransission_Timer, RETRANSMISSION_TIMER);
        start_timer(Heartbeat_Timer, HB_TIMER);

    } else {
        perror("wrong state to send HB, stilll waiting for ack\n");
    }
}

int handle_set_colum(set_colum msg, int fd) {
    //ignore lower seq number, are retransmitted packages
    if(!validate_set_colum(msg)){
        sprintf(errormessage, "MALFORMED SET_COLUM PACKAGE");
        send_error_message(fd, errormessage);
        return 0;
    }
    if(ntohl(msg.seq)==next_seq_opp-1){
        game_message_container msg_c;
        msg_c.set_col_ack = create_set_colum_ack(msg);

        send_game_message(fd, msg_c.message);
        return 0;
    }
    if (ntohl(msg.seq) < next_seq_opp) {
	    sprintf(errormessage, "SEQUENZ NUMBER TOO LOW %d", ntohl(msg.seq));
        send_error_message(fd, errormessage);
        return 0;
    }
    if (current_game_state != OPPONENT_TURN) {
        sprintf(errormessage, "NOT YOUR TURN PLAYER %d", opponent_player);
        send_error_message(fd, errormessage);
        return 0;
    }
    if (ntohl(msg.seq) > next_seq_opp) {
        sprintf(errormessage, "SEQUENZ NUMBER TOO HIGH %d", ntohl(msg.seq));
        send_error_message(fd, errormessage);
        return 0;
    }
    int column = (msg.colum);
    printf("column is *** %d\n", column);
    if (!valid_move(column)) {
        if (column >= NUMBER_OF_COLUMNS || column < 0) {
            sprintf(errormessage, "COLUM %d DOESNT EXIST", column);
            send_error_message(fd, errormessage);
        } else {
            sprintf(errormessage, "COLUM %d IS FULL", column);
            send_error_message(fd, errormessage);
        }
    }

    make_4win_move(column, opponent_player);

    game_message_container msg_c;
    msg_c.set_col_ack = create_set_colum_ack(msg);

    send_game_message(fd, msg_c.message);

    next_seq_opp++;
    current_game_state = PLAYER_TURN;
    if (winner() != 0) {
        handle_winner();
    }
    return 0;
}

int handle_set_colum_ack(set_colum_ack msg, int fd) {
    if(!validate_set_colum_ack(msg)){
        sprintf(errormessage, "MALFORMED SET_COLUM_ACK PACKAGE");
        send_error_message(fd, errormessage);
    }
    //ignore older message
    if (ntohl(msg.seq) < next_seq) {
        return 0;
    }
    if (current_game_state != WAITING_FOR_SET_ACK) {
        sprintf(errormessage, "NOT WAITING FOR SET ACK");
        send_error_message(fd, errormessage);
    }
    if (ntohl(msg.seq) > next_seq) {
        sprintf(errormessage, "SEQUENZ NUMBER %d TOO BIG", ntohl(msg.seq));
        send_error_message(fd, errormessage);
    }
    make_4win_move(last_move, this_player);
    if (winner() != 0) {
        handle_winner();
    }
    next_seq++;
    reset_retransmission(&retransmission_count, &Retransmission_Timer);
   // stop_timer(Retransmission_Timer);
    current_game_state = OPPONENT_TURN;
    printf("[+] Waiting for opp...\n");
    return 0;
}

int handle_heartbeat(heartbeat msg, int fd) {
    heartbeat_ack hb = create_hb_ack(msg);
    send_game_message(fd, *(game_message *) &hb);
    return 0;
}

int handle_heartbeat_ack(heartbeat_ack msg, int fd) {
    if (msg.hdr.length != ntohs(last_heartbeat.hdr.length)) {
        sprintf(errormessage, "HEARTBEAT LENGTHS NOT THE SAME: EXPECTED : %d BUT GOT %d",
                ntohs(last_heartbeat.hdr.length), msg.hdr.length);
        send_error_message(fd, errormessage);
    }

    if (strncmp((char *) msg.heartbeat, (char *) last_heartbeat.heartbeat, msg.hdr.length)) {
        sprintf(errormessage, "HEARTBEATS NOT THE SAME");
        send_error_message(fd, errormessage);
    }
    reset_retransmission(&hb_retransmission_count, &HB_Retransission_Timer);
  //  stop_timer(HB_Retransission_Timer);
    hb_state = HB_READY;
    return 0;
}


int handle_registration_req_ack(registration_req_ack msg, int fd) {
    if (current_game_state != WAITING_FOR_PLAYER_REQ_ACK) {
        sprintf(errormessage, "NOT WAITING FOR PLAYER REQ ACK");
        send_error_message(fd, errormessage);
    }
    reset_retransmission(&retransmission_count, &Retransmission_Timer);
    //stop_timer(Retransmission_Timer);
    current_game_state = WAITING_FOR_PEER_INFO;
    return 0;
}

int handle_peer_info(peer_info msg, int fd) {
    if(!validate_peer_info(msg)){
        sprintf(errormessage, "MALFORMED PEER_INFO PACKAGE");
        send_error_message(fd, errormessage);
        return 0;
    }
    if (current_game_state != WAITING_FOR_PEER_INFO) {
        sprintf(errormessage, "NOT WAITING FOR PEER INFO");
        send_error_message(fd, errormessage);
    }
    game_message_container m;
    m.peer_info_ack = create_peer_info_ack();
    send_game_message(fd,m.message);
    
    char username[USERNAME_SIZE + 1];
    uint32_t ipv4 = ntohl(msg.ip4_addr);
    uint16_t port = ntohs(msg.port);
    char ip[32];
    char p[16];
    memcpy(username, msg.username, msg.hdr.length - 8);
    username[msg.hdr.length - 8] = '\0';
    printf("Trying to connect to : %s (%u %d)\n", username, ipv4, port);
    if (!connected) {
        sprintf(p, "%d", port);
        sprintf(ip, "%u", ipv4);
        //kill connection with server.
        connect_with_peer(&bound_fd, ip, p);

    }
    turn_order(ntohs(msg.start_flag));
    destroy_server(fd);
    setup_game(&bound_fd);
    return 0;
}

void handle_game_message(game_message msg, int fd) {
    msg.hdr.length = ntohs(msg.hdr.length);
    msg.hdr.tag = ntohs(msg.hdr.tag);
    game_message_container msg_container;
    msg_container.message = msg;
    switch (msg.hdr.tag) {
        case SET_COLUM:
            handle_set_colum(msg_container.set_col, fd);
            break;
        case SET_COLUM_ACK:
            handle_set_colum_ack(msg_container.set_col_ack, fd);
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
            sprintf(errormessage, "PEER SHOULD NOT RECEIVE REGISTRATION_REQ PACKAGE");
            send_error_message(fd, errormessage);
            break;
        case REGISTRATION_REQ_ACK:
            handle_registration_req_ack(msg_container.reg_ack, fd);
            break;
        case PEER_INFO:
            handle_peer_info(msg_container.peer_info, fd);
            break;
        case PEER_INFO_ACK:
            sprintf(errormessage, "PEER SHOULD NOT RECEIVE PEER_INFO_ACK PACKAGE");
            send_error_message(fd, errormessage);
            break;
        default:
            sprintf(errormessage, "NOT A RECOGNIZED MESSAGE TAG: %d", msg.hdr.tag);
            send_error_message(fd, errormessage);
    }
}

//UDP (DATAGRAM RECEIVE METHOD)
void receive_peer_message(void *arg) {
    int fd, len;
    game_message msg;
    fd = *(int *) arg;

    if (!connected) {
        struct sockaddr_in addr;
        socklen_t addr_len = sizeof(addr);
        char port[16];
        printf("Trying to connect i guess \n");
        Recvfrom(fd, NULL, 0, MSG_PEEK, (struct sockaddr *) &addr, &addr_len);
        sprintf(port, "%d", ntohs(addr.sin_port));
        connect_with_peer(&fd, inet_ntoa(addr.sin_addr), port);
        printf("connected to %s on port : %s\n", inet_ntoa(addr.sin_addr), port);
        turn_order(0);
    }
    len = Recv(fd, (void *) &msg, sizeof(msg), 0);
    //Message not long enough to read anything
    if (len < HEADER_SIZE) {
        return;
    }
    //printf("receiving message with tag %d and size %d \n", ntohs(msg.hdr.tag), ntohs(msg.hdr.length));
    //not a whole message
    if (len - (HEADER_SIZE + calculate_pad_size(ntohs(msg.hdr.length))) != ntohs(msg.hdr.length)) { // Überprüfung der tatsächlichen Größe des Paketes
        perror("not matching sizes \n");
        return;
    }
    handle_game_message(msg, fd);
}

//TCP (Datastream receive method)
void receive_server_message(void *arg) {
    int fd, len;
    static int offset;


    fd = *(int *) arg;
    len = Recv(fd, (void *) (tcp_buffer + offset), sizeof(game_message) - offset, 0);
    //printf("receiving message from server with len : %d\n",len);
    //NOTHING GOTTEN
    if (len < 0) {
        return;
    }
    offset += len;
    while (offset >= HEADER_SIZE) {
        message_hdr *hdr = (message_hdr *) tcp_buffer; // Der Header der Nachricht wird aus dem Puffer extrahiert 
        //printf("message was with tag: %u and size %u \n",ntohs(hdr->tag),ntohs(hdr->length));
        int pad_size = calculate_pad_size(ntohs(hdr->length)); 
        int msg_len = ntohs(hdr->length) + pad_size + HEADER_SIZE; // Berechnung der erwarteten Nachrichtgröße (Gesamtgröße der Nachricht)
        //Not a whole message
        if (msg_len > offset) { // Überprüfen auf vollständige Nachricht
            return;
        }
        handle_game_message(*(game_message *) tcp_buffer, fd); // Vorbereitung der Nachricht
        memmove(tcp_buffer, tcp_buffer + msg_len, offset - msg_len); // Puffer-Aktualiserung : Um Platz für weitere Daten zuschaffen
        offset -= msg_len;
    }
    return;
}

void game_state_mashine(void *arg) {
    int fd, len;
    char input[5];
    uint16_t colum = 0;
    fd = *(int *) arg;
    len = Read(STDIN_FILENO, (void *) &input, sizeof(input));
    fflush(STDIN_FILENO);
    colum = atoi((char *) &input);
    printf("turn read \n");
    if (current_game_state == PLAYER_TURN && len > 0) {
        if (!valid_move(colum)) {
            printf("illegal move %hu \n", colum);
            return;
        }
        last_move = colum;
        game_message_container msg;
        msg.set_col = create_set_colum(colum, next_seq);
        send_game_message(fd, msg.message);
        last_message = msg.message;
        start_timer(Retransmission_Timer, RETRANSMISSION_TIMER);
        current_game_state = WAITING_FOR_SET_ACK;
    }
    else{
        printf("not your turn  ;( \n");
    }
}

void server_state_mashine(void *arg) {
    int fd;
    char input[USERNAME_PASSWORD_SIZE];

    fd = *(int *) arg;
    Read(STDIN_FILENO, (void *) &input, sizeof(input));
    fflush(STDIN_FILENO);
    printf("input read \n");
    if (current_game_state == INIT) {

        char *username = strtok(input, " ");
        char *password = strtok(NULL, "");
        if(password == NULL){
            printf("please input username and password seperated by a space \n");
            return;
        }
        struct sockaddr_in sa;

        //printf("%s \n", bound_ip);
        inet_pton(AF_INET, bound_ip, &(sa.sin_addr));

        uint32_t ipv4 = ntohl(sa.sin_addr.s_addr);
        uint16_t port = bound_port;
       // printf("ip4 %u port %u addrlen %d \n",ipv4,port,slen);
        registration_req req = create_registration_req(ipv4,port,username, password);

        game_message_container msg;
        msg.reg = req;
        send_game_message(fd, msg.message);
        last_message = msg.message;
        current_game_state = WAITING_FOR_PLAYER_REQ_ACK;
        start_timer(Retransmission_Timer, RETRANSMISSION_TIMER);
    }
}
int setup_game(int *fd) {
    //printf("setting up fd: %d \n", *fd);
    init_4clib();
    hb_state = HB_READY;
    next_seq = 0;
    next_seq_opp = 0;
    Heartbeat_Timer = create_timer(heartbeat_timer, (void *) fd, "heartbeat");
    Retransmission_Timer = create_timer(retransmission_timer, (void *) fd, "retransmission");
    HB_Retransission_Timer = create_timer(heartbeat_retransmission_timer, (void *) fd, "Heartbeat retransmission");

    //register_fd_callback(fd, receive_peer_message, (void *)&fd);
    register_stdin_callback(game_state_mashine, (void *) fd);

    start_timer(Heartbeat_Timer, HB_TIMER);
    return 0;
}

// When binding the udp port be ready to receive messages.
int setup_bound_port(int *fd) {

    register_fd_callback(*fd, receive_peer_message, (void *) fd);
    return 0;
}

int setup_server_connection(int *fd) {

    hb_state = HB_READY;
    //send heartbeat to server
    Heartbeat_Timer = create_timer(heartbeat_timer, (void *) fd, "heartbeat");
    //retransmit Hb to server lol retransmission in TCP connection 
    HB_Retransission_Timer = create_timer(heartbeat_retransmission_timer, (void *) fd, "Heartbeat retransmission");
    //buffer for tcp messages
    tcp_buffer = (char *) calloc(1, (size_t)
    sizeof(game_message));

    //read server messages
    register_fd_callback(*fd, receive_server_message, (void *) fd);

    register_stdin_callback(server_state_mashine, (void *) fd);

    //start_timer(Heartbeat_Timer, HB_TIMER);
    //handle_events();

    return 0;
}

int destroy_server(int fd) {
    stop_timer(Heartbeat_Timer);
    stop_timer(Retransmission_Timer);
    stop_timer(HB_Retransission_Timer);
    delete_timer(Heartbeat_Timer);
    delete_timer(Retransmission_Timer);
    delete_timer(HB_Retransission_Timer);
    deregister_fd_callback(fd);
    deregister_stdin_callback();
    free(tcp_buffer);
    Close(fd);
    return 1;
}

int destroy(int fd) {
    delete_timer(Heartbeat_Timer);
    delete_timer(Retransmission_Timer);
    delete_timer(HB_Retransission_Timer);
    deregister_fd_callback(fd);
    deregister_stdin_callback();
    return 1;
}

int open_udp_socket(int *fd, char *bindport) {
    struct addrinfo *srcres, hints;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    hints.ai_flags = AI_PASSIVE;

    getaddrinfo(NULL, bindport, &hints, &srcres);
    *fd = Socket(srcres->ai_family, srcres->ai_socktype, srcres->ai_protocol);

    Bind(*fd, srcres->ai_addr, srcres->ai_addrlen);
    bound_port = atoi(bindport);
    setup_bound_port(fd);
    return *fd;
}

int connect_with_peer(int *fd, char *hostname, char *servname) {
    struct addrinfo *destres = NULL, hints;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    getaddrinfo(hostname, servname, &hints, &destres);

    //destres
    int succ = 0;
    while (destres != NULL) {
        if (connect(*fd, (const struct sockaddr *) destres->ai_addr, destres->ai_addrlen) != -1) {
            succ = 1;
            break;
        }
        destres = destres->ai_next;
    }
    if (succ != 1) {
        perror("couldn't connect to this ip \n");
        exit(-1);
    }
    connected = 1;
    return 0;
}

int connect_with_server(int *fd, char *hostname, char *servname) {
    struct addrinfo *destres = NULL, hints;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    getaddrinfo(hostname, servname, &hints, &destres);
    //destres
    int succ = 0;
    while (destres != NULL) {
        *fd = Socket(destres->ai_family, destres->ai_socktype, destres->ai_protocol);
        if (connect(*fd, (const struct sockaddr *) destres->ai_addr, destres->ai_addrlen) != -1) {
            succ = 1;
            break;
        }
        destres = destres->ai_next;
    }
    if (succ != 1) {
        perror("couldn't connect to this ip \n");
        exit(-1);
    }
    setup_server_connection(fd);
    return 0;
}

/**
 * arv[1] = hostname (ip des servers);
 * argv[2] = servname (port des servers);
 * argv[3] = bindport (port des peers); 
 * * (optional) argv[4] = ip des peers. (ohne eingabe einfach 127.0.0.1)
 *
 * keine eingabe : 
 */

int main(int argc, char **argv) {
    char *servname, *hostname, *bindport;
    //uint16_t startflag = 0;
    if (argc == 1) {
        hostname = "127.0.0.1";
        servname = "9999";
        bindport = "7778";
        bound_ip = "127.0.0.1";
        //startflag = 1;
    } else if (argc == 4) {
        hostname = argv[1];
        servname = argv[2];
        bindport = argv[3];
        bound_ip = "127.0.0.1";
        //startflag = 1;
    }else if(argc == 5){
        hostname = argv[1];
        servname = argv[2];
        bindport = argv[3];
        bound_ip = argv[4];
    }
    
    else {
        perror("wrong use of parameters \n");
        exit(-1);
    }
    init_cblib();
    int peer_fd = 0;
    int server_fd = 0;
    open_udp_socket(&peer_fd, bindport);
    bound_fd = peer_fd;
    connect_with_server(&server_fd, hostname, servname);

    //connect_with_peer(&peer_fd,hostname,servname);
    //turn_order(startflag);
    //setup_game(&bound_fd);
    printf("username and password, seperated by space\n");
    handle_events();
}
