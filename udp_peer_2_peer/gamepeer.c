/**
 * NOTES:
 *  the set_colunm ack is sent with thw same sequence number ...
 *  to change it or increment it:
 *      go to gameprotocol.c > create_set_column_ack > and increment the sequence from the incoming before assigning ...
 */

#define _POSIX_C_SOURCE 200112L
#define _XOPEN_SOURCE 600

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include "../util/game_setup/4clib.h"
#include "../util/wrapper/Socket.h"
#include "../util/reactor_setup/cblib.h"
#include <locale.h>
#include "gamepeer.h"
#include <signal.h>
#include "../util/protocol/gameprotocoll.h"
#include "gamepeer.h"
#include <signal.h>

#define DEBUG 1

#define BUFFER_SIZE (1 << 16)
// all timers
static struct timer *heartbeat_hook, *retransmission_hook, *hb_retransission_hook;
// saves who is which player and the seq number.
int this_player, opponent_player, next_seq;
// the current game state
state current_game_state;
// the state of the heartbeat
state hb_state;
// error message char* only here for comfort.
char errormessage[ERRORMESSAGE_SIZE];


// saves last move for doing it when ack is recieved
uint16_t last_move = 0;

// saves last heartbeat message for validation.
heartbeat last_heartbeat;

// saves last game_message for retransmission
game_message last_message;

// connected flag to check if the udp socket is already connected to another peer
uint8_t connected = 0;
// the fd of the bound port.
int bound_fd=0;

// counts the amount of retransmissions for calculating the next timer and checking if too many retransmission
int retransmission_count = 0;
int hb_retransmission_count = 0;






void destruct() { // this will be called in case of receiving STOP signal ...
    stop_timer(retransmission_hook);
    stop_timer(heartbeat_hook);
    stop_timer(hb_retransission_hook);
    delete_timer(heartbeat_hook);
    heartbeat_hook = NULL;
    delete_timer(retransmission_hook);
    retransmission_hook = NULL;
    delete_timer(hb_retransission_hook);
    hb_retransission_hook = NULL;
    deregister_stdin_callback();
}

void handle_stop(int fd) {
    deregister_fd_callback(fd);
    destruct();
    exit(EXIT_SUCCESS);
}
/**
 * connects a peer with another peer with ip hostname and port servname on fd fd
 */
int udp_establish_conn(int *fd, char *hostname, char *servname) {
    struct addrinfo *destres = NULL, *res = NULL, hints;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    Getaddrinfo(hostname, servname, &hints, &res);

    destres = res;
    int succ = 0;
    while (destres != NULL) {
        if (connect(*fd, (const struct sockaddr *) destres->ai_addr, destres->ai_addrlen) != -1) {
            succ = 1;
            printf("[+] peer connected ... \n");
            break;
        }
        destres = destres->ai_next;
    }
    if (succ != 1) {
        perror("couldn't connect to this ip \n");
        exit(-1);
    }
    connected = 1;
    freeaddrinfo(res);
    return 0;
}
/**
 * calculates the current retransmission timer.
 */
int calculate_retransmission_time(int retransmission_count){
    if(retransmission_count >RETRANSMISSION_MAX){
        perror("too many retransmissions");
        exit(-1);
    }
    int res = RETRANSMISSION_TIMER;
    for(int i= 0; i<retransmission_count;i++){
        res=res*2;
    }
    return res;
}
/**
 * generic retransmission method
 */
void retransmission_timer(void *arg) {
    int fd;
    fd = *(int *) arg;
    retransmission_count++;
    printf("retransmission of message with Tag %hu\n", ntohs(last_message.hdr.tag));
    send_game_message(fd, last_message);
    start_timer(retransmission_hook, calculate_retransmission_time(retransmission_count));
}

/**
 * message for retransmitting heartbeats.
 */
void heartbeat_retransmission_timer(void *arg) {
    int fd;
    fd = *(int *) arg;
    hb_retransmission_count++;
    printf("retransmission of heartbeat: %s \n", last_heartbeat.heartbeat);
    game_message_container msg;
    msg.hb = last_heartbeat;
    send_game_message(fd, msg.message);
    start_timer(hb_retransission_hook, calculate_retransmission_time(hb_retransmission_count));
}

/**
 * method that sends a heartbeat after timer runs out in cblib.
 * starts retransmission timer for that heartbeat and starts a new heartbeat timer.
 */
void heartbeat_timer(void *arg) {
    int fd;

    fd = *(int *) arg;
    if (hb_state == HB_READY) {
        heartbeat hb;
        hb = create_hb("heartbeat");
        last_heartbeat = hb;
        //printf("heartbeat header length about to send: %d\n", ntohs(last_heartbeat.hdr.length) );
        sleep(1);
        send_game_message(fd, *(game_message *) &hb);
        hb_state = WAITING_FOR_HB_ACK;



        start_timer(hb_retransission_hook, RETRANSMISSION_TIMER);
        start_timer(heartbeat_hook, HB_TIMER);

    } else {
        perror("wrong state to send HB, still waiting for ack\n");
    }
}

/**
 * stdin callback for playing a game with a client
 */
void stdin_handler(void *arg) {
    int fd, len;
    char input[6];
    uint8_t colum = 0;
    fd = *(int *) arg;
//    printf("size = %d \n", sizeof(input));
    len = Read(STDIN_FILENO, (void *) &input, sizeof(input));
    //printf("!!! len = %d\n", len);
    fflush(STDIN_FILENO);
    input[len] = 0;
//    printf("len = %d \n", len);
    if (current_game_state == PLAYER_TURN && len > 0) {
        if (!strncmp((char*)input, "quit", 4)) {
            game_message_container msg ;
            msg.stop_msg = create_stop_msg();
            printf("try to stop the game in other peer ...\n");
            send_game_message(fd, msg.message);
            printf("game stoped ...\n");
            handle_stop(fd);
        }

        colum = atoi((char *) &input);
        if (!(valid_move(colum)) || len > 2) {
            printf("illegal move %hu \n", colum);
            return;
        }

        last_move = colum;
        game_message_container msg;
        printf("[+] Sending move: %d, seq: %d\n", colum, next_seq);
        msg.set_col = create_set_colum(colum, next_seq);
        send_game_message(fd, msg.message);
        last_message = msg.message;
        start_timer(retransmission_hook, RETRANSMISSION_TIMER);
        current_game_state = WAITING_FOR_SET_ACK;
    }
    else if (len>0){
        printf("not your turn  ;( \n");
    }
    else{
        printf("empty input \n");
    }
}
/**
 * prepares the game to be played, inits a new 4clib and sets all states accordingly for the game to start.
 */
int init_game(int *fd) {
    init_4clib();
    hb_state = HB_READY;
    next_seq = 0;
    heartbeat_hook = create_timer(heartbeat_timer, (void *) fd, "heartbeat");
    retransmission_hook = create_timer(retransmission_timer, (void *) fd, "retransmission"); // Global variables ...
    hb_retransission_hook = create_timer(heartbeat_retransmission_timer, (void *) fd, "Heartbeat retransmission"); // Global Variables ...
    register_stdin_callback(stdin_handler, (void *) fd);

    start_timer(heartbeat_hook, HB_TIMER);
    return 0;
}

/**
 * prints who won.
 */
void handle_winner() {
    if (winner() == this_player) {
        printf("\r 🎉 YOU WON!!! \n");
    } else {
        printf("\r YOU LOST 💔!!!\n");
    }
    exit(0);
}
/**
 * makes a move in game 4 connect.
 * prints board.
 */
void apply_move(int colum, int player) {
    if (!valid_move(colum)) {
        perror("Something went wong that is not a valid move");
        return;
    }
    // system("clear");
    make_move(colum, player);
    print_board();
    //printf("[+] move made; current game state : %d\n", current_game_state);
    if (current_game_state == OPPONENT_TURN)
        printf("[+] Waiting for opp...\n");

}

/**
 * you start
 * sets values accordingly
 */
void this_starts() {
    opponent_player = PLAYER_2;
    this_player = PLAYER_1;
    current_game_state = PLAYER_TURN;
    printf("you start \n");
}
/**
 * the opponent starts
 * sets values accordingly
 */
void opponent_starts() {
    opponent_player = PLAYER_1;
    this_player = PLAYER_2;
    current_game_state = OPPONENT_TURN;
    printf("opponent starts \n");
}
/**
 * sets the turn order and prints which player you are.
 */
void turn_determination(uint16_t startflag) {
    if (startflag) {
        this_starts();
    } else {
        opponent_starts();
    }
    //printf("This player: %d \n", this_player);
}
/**
 * resets retransmission count and stops retransmission timer for the specifyed timer and count
 */
void reset_retransmission(int * retransmission_count, struct timer** timer){
    *retransmission_count=0;
    stop_timer(*timer);
}


/**
 * handles a set colum message
 * checks if move is valid
 * does the move
 * sends a set colum ack message back
 */
int handle_set_colum(set_colum msg, int fd) {
    //ignore lower seq number, are retransmitted packages
    if(!validate_set_colum(msg)){
        sprintf(errormessage, "MALFORMED SET_COLUM PACKAGE");
        send_error_message(fd, errormessage);
    }
    printf("[+] Received move: %d , seq : %d\n", msg.colum,ntohl(msg.seq));
    if(ntohl(msg.seq)==next_seq-1){
        printf("___entred that myserious if\n");
        game_message_container msg_c;
        msg_c.set_col_ack = create_set_colum_ack(msg);
        send_game_message(fd, msg_c.message);
//        current_game_state = OPPONENT_TURN;
        return 0;
    }
    if (ntohl(msg.seq) < next_seq) {
        printf("seq problem\n");
        return 0;
    }
    if (current_game_state != OPPONENT_TURN) {
        sprintf(errormessage, "NOT YOUR TURN PLAYER %d", opponent_player);
        send_error_message(fd, errormessage);
    }
    /*if (ntohl(msg.seq) > next_seq) {
        printf("msg.seq= %d\t next seq = %d\n", ntohl(msg.seq), next_seq);
        sprintf(errormessage, "SEQUENZ NUMBER TOO HIGH %d", ntohl(msg.seq));
        send_error_message(fd, errormessage);
    }*/
    if (!valid_move((msg.colum))) {
        if ((msg.colum) >= NUMBER_OF_COLUMNS || (msg.colum) < 0) {
            sprintf(errormessage, "COLUM %d DOESNT EXIST", (msg.colum));
            send_error_message(fd, errormessage);
        } else {
            sprintf(errormessage, "COLUM %d IS FULL", (msg.colum));
            send_error_message(fd, errormessage);
        }
    }

    current_game_state = PLAYER_TURN;
    //printf("move received: %d\n", msg.colum);
    //printf("[+] game_state = %d; \n", current_game_state);
    apply_move((msg.colum), opponent_player);

    game_message_container msg_c;
    msg_c.set_col_ack = create_set_colum_ack(msg);

    send_game_message(fd, msg_c.message);


    next_seq++;



    if (winner() != 0) {
        handle_winner();
    }
    return 0;
}
/**
 * handles a set colum ack messagemsg, ID SET_
 * does the acked move
 * checks if one player won
 */
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

    current_game_state = OPPONENT_TURN;
    //printf("[+] game_state = %d; \n", current_game_state);
    apply_move(last_move, this_player);

    if (winner() != 0) {
        handle_winner();
    }
//    next_seq++;
    reset_retransmission(&retransmission_count, &retransmission_hook);

    return 0;
}
/**
 * handles a heartbeat message and sends a heartbeat ack back.
 */
int handle_heartbeat(heartbeat msg, int fd) {
    heartbeat_ack hb = create_hb_ack(msg);
    send_game_message(fd, *(game_message *) &hb);
    return 0;
}
/**
 * handles heartbeat ack message (and stops retransmission timer for that heartbeat)
 */
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
    reset_retransmission(&hb_retransmission_count, &hb_retransission_hook);
    hb_state = HB_READY;
    return 0;
}


/**
 * gets a game_message and passes it on to the right handle method for that message tag.
 */
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
        case STOP:
            printf("Stop event is catched ...\n");
            handle_stop(fd);
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
        udp_establish_conn(&fd, inet_ntoa(addr.sin_addr), port);
        printf("connected to %s on port : %s\n", inet_ntoa(addr.sin_addr), port);
        turn_determination(0);
    }
    len = Recv(fd, (void *) &msg, sizeof(msg), 0);
    //Message not long enough to read anything
    if (len < HEADER_SIZE) {
        return;
    }
    int new2 = len - (HEADER_SIZE + calculate_pad_size(ntohs(msg.hdr.length)));
    //int old = ntohs(msg.hdr.length);
    //printf("[+] len received: %d\n", len);

    //printf("[+] msg.hdr.len = %d\n", ntohs(msg.hdr.length));
    //printf("[+] len - (4  + %d) : %d\n", calculate_pad_size(ntohs(msg.hdr.length)), new2);
    //printf("[*] %d\n", calculate_pad_size(ntohs(msg.hdr.length)));
    //printf("[+] msg.hdr.tag = %d\n", ntohs(msg.hdr.tag));
    //printf("[+]----------------[+]\n");

    //printf("[+]current_gamne_message: %d\n", current_game_state);


    //not a whole message
    /*if (len - (HEADER_SIZE + calculate_pad_size(ntohs(msg.hdr.length))) != ntohs(msg.hdr.length)) {
        perror("not matching sizes \n");
        //return;
        close(fd);
        exit(-1);
    }*/

    handle_game_message(msg, fd);
}


// When binding the udp port be ready to receive messages.
int setup_bound_port(int *fd) {

    register_fd_callback(*fd, receive_peer_message, (void *) fd);
    return 0;
}



/**
 * opens an udp socket on port bindport and ip IPANY
 */
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
    setup_bound_port(fd);
    return *fd;
}

// Connect peer to peer use :
// peer: ./gamepeer <peer_host_ip> <peer_port_number> <this_peer_port> -server
// peer:./gamepeer <peer_host_ip> <peer_port_number> <this_peer_port> -client
int start;
void print_test(){
    printf("%ld\n", time(NULL) - start);
}


int main(int argc, char **argv) {
    char *servname, *hostname, *bindport;
    uint16_t startflag = 0;
    if (argc == 5) {
        hostname = argv[1];
        servname = argv[2];
        bindport = argv[3];

        if (strcmp(argv[4], "-server") == 0)
            startflag = 0;
        else startflag = 1;

    }else {
        perror("<usage>: ./prog peer_host_ip peer_port_number this_peer_port . \n");
        exit(EXIT_FAILURE);
    }


    setlocale(LC_ALL, "");
    init_cblib();
    int peer_fd = 0;
    open_udp_socket(&peer_fd, bindport); // register socket fd
    bound_fd = peer_fd;
    udp_establish_conn(&peer_fd,hostname,servname);
    init_game(&bound_fd); // register stdin ...
    turn_determination(startflag);
    handle_events();
}

