cmake_minimum_required(VERSION 4.2)
project(vier_gewinnt C)

set(CMAKE_C_STANDARD 11)

include_directories(.)
include_directories(util)

add_executable(vier_gewinnt_tcp_server
        util/game_setup/4clib.c
        util/game_setup/4clib.h
        util/reactor_setup/cblib.h
        util/reactor_setup/cblib.c
        util/protocol/gameprotocoll.c
        util/protocol/gameprotocoll.h
        util/wrapper/Socket.c
        util/wrapper/Socket.h
        tcp_client_server/tcp_gameserver.c
        tcp_client_server/tcp_gameserver.h)


add_executable(vier_gewinnt_tcp_client
        util/game_setup/4clib.c
        util/game_setup/4clib.h
        util/reactor_setup/cblib.h
        util/reactor_setup/cblib.c
        util/protocol/gameprotocoll.c
        util/protocol/gameprotocoll.h
        util/wrapper/Socket.c
        util/wrapper/Socket.h
        tcp_client_server/tcp_gamepeer_client.c
        )


add_executable(vier_gewinnt_udp_peer
        util/game_setup/4clib.c
        util/game_setup/4clib.h
        util/reactor_setup/cblib.h
        util/reactor_setup/cblib.c
        util/protocol/gameprotocoll.c
        util/protocol/gameprotocoll.h
        util/wrapper/Socket.c
        util/wrapper/Socket.h
        udp_peer_2_peer/gamepeer.h
        udp_peer_2_peer/gamepeer.c
)