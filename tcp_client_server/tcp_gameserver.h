#ifndef NETZWERKPROGRAMMIERUNG_GAMESERVER_H
#define NETZWERKPROGRAMMIERUNG_GAMESERVER_H

#include "../util/protocol/gameprotocoll.h"

typedef struct pinfo {
    uint32_t ip4_addr;
    uint16_t port;
    uint8_t username[USERNAME_SIZE];
    uint32_t username_len;
    uint8_t password[USERNAME_SIZE];
} pinfo;

typedef enum state {
    INIT,
    WAITING_FOR_PLAYER_REQ,
    WAITING_FOR_PLAYERS,
    WAITING_FOR_PEER_INFO_ACK,
    HB_READY,
    WAITING_FOR_HB_ACK
} state;


#endif //NETZWERKPROGRAMMIERUNG_GAMESERVER_H
