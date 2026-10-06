#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "../game_setup/4clib.h"
#include "../wrapper/Socket.h"
#include "../reactor_setup/cblib.h"

#include "gameprotocoll.h"

/**
 * sends generic game_message
 */
void send_game_message(int fd, game_message msg) {
    int pad = calculate_pad_size(ntohs(msg.hdr.length));
    add_pad(&msg);
    //printf("sending message with tag %d ,size %d and pad  \n", ntohs(msg.hdr.tag), ntohs(msg.hdr.length));
    Send(fd, &msg, ntohs(msg.hdr.length) + HEADER_SIZE + pad, 0);
}

/**
 * calculates the pad size
 */
int calculate_pad_size(uint16_t len) {
    return (ALIGNMENT - (len % ALIGNMENT)) % ALIGNMENT;
}
/**
 * adds the pad to a game_message
 */
void add_pad(game_message *msg) {
    int len = ntohs(msg->hdr.length);
    int padlen = calculate_pad_size(ntohs(msg->hdr.length));
    for (int i = 0; i < padlen; i++) {
        msg->message[len + i] = 'A' + i;
    }
    return;
}
/**
 * following are all the methods for creating a specific kind of message
 */
message_hdr create_hdr(uint16_t tag, uint16_t len) {
    message_hdr ret;
    ret.length = htons(len);
    ret.tag = htons(tag);
    return ret;
}

heartbeat create_hb(char *msg) {
    heartbeat hb;
    uint8_t message[MESSAGE_SIZE];
    size_t len;
    len = strlen(msg);

    hb.hdr = create_hdr(HEARTBEAT, (uint16_t) len);
    memcpy(message, msg, 1 + sizeof(char) * len);
    memcpy(hb.heartbeat, message, len);


    /**
    for (int i = 0; i < MESSAGE_SIZE; i++) {
            hb.heartbeat[i] = 33 + (i % 95);
        }
        */
    return hb;
}

heartbeat_ack create_hb_ack(heartbeat incoming) {
    heartbeat_ack hb;
    hb.hdr = create_hdr(HEARTBEAT_ACK, incoming.hdr.length);
    memcpy(hb.heartbeat, incoming.heartbeat, (size_t) incoming.hdr.length);
    return hb;
}

set_colum create_set_colum(uint8_t colum, uint32_t seq) {
    set_colum sc;
    sc.hdr = create_hdr(SET_COLUM, 5);
    sc.seq = htonl(seq);
    sc.colum = colum;
    return sc;
}

set_colum_ack create_set_colum_ack(set_colum incoming) {
    set_colum_ack sc;
    sc.hdr = create_hdr(SET_COLUM_ACK, 4);
    sc.seq = incoming.seq;
    return sc;
}

error create_error(char message[ERRORMESSAGE_SIZE], uint16_t length) {
    error er;
    //PLUS ERRORTYPE SIZE
    er.hdr = create_hdr(ERROR, length + 4);
    // er.error_type = htonl(error_type);
    memcpy(er.error_msg, message, (size_t) length);
    perror(message);
    return er;
}

registration_req create_registration_req(uint32_t ipv4, uint16_t port, char *username, char *password) {
    registration_req req;
    uint16_t password_len = strlen(password);
    uint16_t username_len = strlen(username);
    //TODO 10
    req.hdr = create_hdr(REGISTRATION_REQ, username_len + password_len + 10);
    req.ip4_addr = htonl(ipv4);
    req.port = htons(port);
    req.password_len = htons(password_len);
    req.username_len = htons(username_len);

    memset(req.username_password, 0, sizeof(req.username_password));
    memcpy(req.username_password, username, username_len);
    memcpy(req.username_password + username_len, password, password_len);
    return req;
}

registration_req_ack create_registration_req_ack() {
    registration_req_ack req;
    req.hdr = create_hdr(REGISTRATION_REQ_ACK, 0);
    return req;
}

peer_info create_peer_info(uint32_t ipv4, uint16_t port, uint16_t start_flag, char *username, uint32_t username_len) {
    peer_info pinfo;
    pinfo.hdr = create_hdr(PEER_INFO, 8 + username_len);
    pinfo.ip4_addr = htonl(ipv4);
    pinfo.port = htons(port);
    pinfo.start_flag = htons(start_flag);
    memset(pinfo.username, 0, sizeof(pinfo.username));
    memcpy(pinfo.username, username, username_len);
    return pinfo;
}

peer_info_ack create_peer_info_ack() {
    peer_info_ack pinfo;
    pinfo.hdr = create_hdr(PEER_INFO_ACK, 0);
    return pinfo;
}
/**
 * validation of message lenghts according to specification. If no implementation the message has no specific condition
 */

int validate_hdr(game_message msg, int len){
    return msg.hdr.length + HEADER_SIZE + calculate_pad_size(ntohs(msg.hdr.length)) == len;
}

int validate_set_colum(set_colum msg){
    return msg.hdr.length == 5;
}
int validate_set_colum_ack(set_colum_ack msg){
    return msg.hdr.length == 4;
}
//int validate_stop(stop_msg msg){
//  return msg.hdr.length == 0;
//}

int validate_error(error msg){
    return msg.hdr.length >= 0;
}
int validate_registration_req(registration_req msg){
    return msg.hdr.length>=10;
}
int validate_peer_info(peer_info msg){
    return msg.hdr.length>=8;
}
/**
 * creates an error message and sends it
 */
void send_error_message(int fd, char *error_msg) {
    game_message_container msg;
    msg.error = create_error(error_msg, strlen(error_msg));
    send_game_message(fd, msg.message);
}


stop_msg create_stop_msg() {
  	stop_msg sc;
    sc.hdr = create_hdr(STOP, 0);
    return sc;
}

/**
 * handles an error message by printing it and exiting the programm.
 */
int handle_error(error msg, int fd) {
    if(!validate_error(msg)){
        char errormessage[ERRORMESSAGE_SIZE];
        sprintf(errormessage, "MALFORMED ERROR PACKAGE");
        send_error_message(fd, errormessage);
    }
    uint8_t buf[ERRORMESSAGE_SIZE + 1];
    memset(buf, 0, sizeof(buf));
    memcpy(buf, msg.error_msg, msg.hdr.length);
    buf[msg.hdr.length - 4] = '\0';
    // switch (ntohl(error_type)) {
    //     case UNKNOWN_TYPE:
    //         perror("unknown type \n");
    //         break;
    //     case COLUM_DOESNT_EXIST:
    //         perror("colum doesnt exist \n");
    //         break;
    //     case COLUM_FULL:
    //         perror("colum full \n");
    //         break;
    //     case PROTOCOLL_ERROR:
    //         perror("protocoll error \n");
    //         break;
    //     default:
    //         perror("unknown ERROR tpye \n");
    // }
    perror((char *) buf);
    exit(-1);
}
