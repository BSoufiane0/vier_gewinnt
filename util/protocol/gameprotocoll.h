#ifndef NETZWERKPROGRAMMIERUNG_GAMEPROTOCOLL_H
#define NETZWERKPROGRAMMIERUNG_GAMEPROTOCOLL_H

#include <stdint.h>

#define ALIGNMENT 4

//#define MESSAGE_SIZE (0xFFFF)
#define MESSAGE_SIZE 2048
#define HEADER_SIZE (4)
#define PACKAGE_SIZE (MESSAGE_SIZE + HEADER_SIZE)
#define ERRORMESSAGE_SIZE (MESSAGE_SIZE - 4)
#define USERNAME_PASSWORD_SIZE (MESSAGE_SIZE - 10)
#define USERNAME_SIZE (MESSAGE_SIZE - 8)

#define RETRANSMISSION_MAX 5
#define RETRANSMISSION_TIMER 1000
#define HB_TIMER 10000

typedef enum message_tag {
    SET_COLUM = 0,
    SET_COLUM_ACK,
    STOP,
    HEARTBEAT,
    HEARTBEAT_ACK,
    ERROR,
    REGISTRATION_REQ,
    REGISTRATION_REQ_ACK,
    PEER_INFO,
    PEER_INFO_ACK
} message_tag;

typedef enum error_type {
    UNKNOWN_TYPE,
    COLUM_DOESNT_EXIST,
    COLUM_FULL,
    PROTOCOLL_ERROR
} error_type;
//default
typedef struct message_hdr {
    uint16_t tag;
    uint16_t length;
} message_hdr;

typedef struct game_message {
    message_hdr hdr;
    uint8_t message[MESSAGE_SIZE];
} game_message;

//1
typedef struct set_colum {
    message_hdr hdr;
    uint32_t seq;
    uint8_t colum;
} set_colum;
//2
typedef struct set_colum_ack {
    message_hdr hdr;
    uint32_t seq;
} set_colum_ack;
//3
typedef struct stop_msg {
    message_hdr hdr;
}stop_msg;
//4
typedef struct heartbeat {
    message_hdr hdr;
    uint8_t heartbeat[MESSAGE_SIZE];
} heartbeat;
//5
typedef struct heartbeat_ack {
    message_hdr hdr;
    uint8_t heartbeat[MESSAGE_SIZE];
} heartbeat_ack;
//6
typedef struct error { // todo : add string error specification
    message_hdr hdr;
    // uint32_t error_type;
    uint8_t error_msg[ERRORMESSAGE_SIZE];
} error;

//7
typedef struct registration_req {
    message_hdr hdr;
    uint32_t ip4_addr;
    uint16_t port;
    uint16_t username_len;
    uint16_t password_len;
    uint8_t username_password[USERNAME_PASSWORD_SIZE];
} registration_req;

//8
typedef struct registration_req_ack {
    message_hdr hdr;
} registration_req_ack;

//9
typedef struct peer_info {
    message_hdr hdr;
    uint32_t ip4_addr;
    uint16_t port;
    uint16_t start_flag;
    char username[USERNAME_SIZE];
} peer_info;

//10
typedef struct peer_info_ack {
    message_hdr hdr;
} peer_info_ack;

typedef union game_message_container {
    game_message message;
    set_colum set_col;
    set_colum_ack set_col_ack;
    heartbeat hb;
    heartbeat_ack hb_ack;
    error error;
    registration_req reg;
    registration_req_ack reg_ack;
    peer_info peer_info;
    peer_info_ack peer_info_ack;
    stop_msg stop_msg;
} game_message_container;

void send_game_message(int fd, game_message msg);

int calculate_pad_size(uint16_t len);

void add_pad(game_message *msg);

message_hdr create_hdr(uint16_t tag, uint16_t len);

heartbeat create_hb(char *msg);

heartbeat_ack create_hb_ack(heartbeat incoming);

set_colum create_set_colum(uint8_t colum, uint32_t seq);

set_colum_ack create_set_colum_ack(set_colum incoming);

error create_error( char message[ERRORMESSAGE_SIZE], uint16_t length);

registration_req create_registration_req(uint32_t ipv4, uint16_t port, char *username, char *password);

registration_req_ack create_registration_req_ack();

peer_info create_peer_info(uint32_t ipv4, uint16_t port, uint16_t start_flag, char *username, uint32_t username_len);

peer_info_ack create_peer_info_ack();

void send_error_message(int fd, char *error_msg);

int handle_error(error msg, int fd);
stop_msg create_stop_msg();

int validate_hdr(game_message msg, int len);
int validate_set_colum(set_colum msg);
int validate_set_colum_ack(set_colum_ack msg);
int validate_error(error msg);
int validate_registration_req(registration_req msg);
int validate_peer_info(peer_info msg);



#endif //NETZWERKPROGRAMMIERUNG_GAMEPROTOCOLL
