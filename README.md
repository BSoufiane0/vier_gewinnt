# Connect Four (Vier Gewinnt) – Network Multi-Mode Implementation in C

An implementation of the classic Connect Four game written in C, designed for multi-host gameplay over local or remote networks. This project showcases socket programming, protocol design, and robust network architectures by offering two distinct operational modes: TCP Client-Server and UDP Peer-to-Peer.
## 🚀 Architectural Overview

The application supports two independent networking modes depending on deployment needs:
1. TCP Client-Server Mode

   Centralized Matchmaking: A dedicated TCP Server acts as an orchestrator, handling client registrations and automatically pairing two online players into an active match.

   Reliable Game State Sync: All move validations, game flow triggers, and state updates pass through a persistent, connection-oriented TCP channel to guarantee ordered, packet-loss-free communication.

2. UDP Peer-to-Peer Mode

   Direct Host-to-Host Gameplay: No intermediary server required—two peers communicate directly over UDP.

   Lightweight Protocol: Game moves and acknowledgment payloads are exchanged via custom UDP messages, reducing infrastructure dependency and connection overhead.

## Key Features

Multi-Host Interoperability: Play seamlessly between different machines and network environments.

Strict Protocol Compliance: Built according to a predefined message specification to ensure effortless cross-compatibility with external team implementations.

Low-Level Socket Programming: Developed purely in C using POSIX Sockets with custom packet parsing and input validation.
Praktikum 4: (UDP peer to peer)

Compiling :

        gcc -o gamepeer gamepeer.c 4clib.c cblib.c Socket.c gameprotocoll.c -pthread
Running:

        ./gamepeer <other_peer_ip> <other_peer_port> <this_peer_port> -client/server
    NOTE:
        starting the peers from the same host does not trigger any troubles,
        but starting the peers from different hosts requires to use
        public ips rather than private one. otherwise the peers won't connect .

Praktikum 5: (TCP client-server)

Compiling tcp_gameserver.c : 

    gcc -o output/tcp_gameserver tcp_client_server/tcp_gameserver.c util/game_setup/4clib.c util/reactor_setup/cblib.c util/wrapper/Socket.c util/protocol/gameprotocoll.c -pthread
Compiling tcp_gamepeer_client.c : 

    gcc -o output/tcp_game_client tcp_client_server/tcp_gamepeer_client.c util/game_setup/4clib.c util/reactor_setup/cblib.c util/wrapper/Socket.c util/protocol/gameprotocoll.c -pthread

Running it localy: 

Server: 
    
    ./output/tcp_gameserver
Peer 1: 
    
    ./output/tcp_game_client
    
Peer 2:
    
    ./output/tcp_game_client 127.0.0.1 9999 7779

Running it remotly :
Server:

    ./output/tcp_gameserver <server_ip> <server_port_number>
Peer 1:

    ./output/tcp_game_client <server_ip> <server_port_number> <this_peer_bound_port> <this_peer_ip>
Peer 2:

    ./output/tcp_game_client <server_ip> <server_port_number> <this_peer_bound_port> <this_peer_ip>