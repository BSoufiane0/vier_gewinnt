#ifndef NETZWERKPROGRAMMIERUNG_GAMEPEER_H
#define NETZWERKPROGRAMMIERUNG_GAMEPEER_H

typedef enum state {
    INIT,
    WAITING_FOR_PLAYER_REQ_ACK,
    WAITING_FOR_PEER_INFO,
    PLAYER_TURN,
    WAITING_FOR_SET_ACK,
    OPPONENT_TURN,
    HB_READY,
    WAITING_FOR_HB_ACK
} state;

int udp_establish_conn(int *fd, char *hostname, char *servname);

int init_game(int *fd);
int connect_with_peer(int *fd, char *hostname, char *servname) ;
int destroy_server(int fd);
int setup_game(int *fd);
#endif //NETZWERKPROGRAMMIERUNG_GAMEPEER_H
