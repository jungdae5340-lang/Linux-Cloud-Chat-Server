#ifndef CHATSERVER_H
#define CHATSERVER_H

#include <iostream>
#include <vector>
#include <mutex>
#include <thread>
#include <string>
#include <algorithm>
#include <cstring> // memset
#include <filesystem>
#include <fstream>
#include <map>
// [Linux Porting] Winsock 제거 및 POSIX 헤더 추가
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <cerrno>  // errno
// [Linux Porting] 타입 매핑
using SOCKET = int;
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket close
// ChatServer 클래스 전방 선언
class ClientHandler;
class ChatServer;

extern ChatServer* g_server_instance;

void handle_client_wrapper(ClientHandler* handler);

class ClientHandler {
private:
    SOCKET client_socket;
    std::string client_ip;
    std::string nickname;
    std::string current_room;

public:
    ClientHandler(SOCKET sock, const std::string& ip)
        : client_socket(sock), client_ip(ip), current_room("Lobby") {
    }

    SOCKET getSocket() const { return client_socket; }
    std::string getIp() const { return client_ip; }
    std::string getNickname() const { return nickname; }
    void setNickname(const std::string& name) { nickname = name; }
    std::string getRoom() const { return current_room; }
    void setRoom(const std::string& room_name) { current_room = room_name; }

    void send_message(const std::string& message) {
        std::string msg_with_newline = message + "\n";
        // [Linux Porting] MSG_NOSIGNAL: 상대방 연결이 끊겨도 프로세스가 종료(SIGPIPE)되지 않도록 함
        send(client_socket, msg_with_newline.c_str(), (int)msg_with_newline.length(), MSG_NOSIGNAL);
    }

    ~ClientHandler() {
        if (client_socket != INVALID_SOCKET) {
            close(client_socket); // [Linux Porting] closesocket -> close
        }
    }
};

class ChatServer {
private:
    SOCKET listen_socket;
    int port;
    std::vector<ClientHandler*> client_list;
    std::mutex client_mutex;
    bool is_running;
    std::map<std::string, std::vector<ClientHandler*>> rooms;

public:
    ChatServer(int p);
    bool init_winsock(); // 리눅스에선 빈 함수가 됨
    bool create_listen_socket();
    void start_listen();
    void process_admin_console();
    void broadcast_message(const std::string& message, ClientHandler* sender = nullptr);
    void send_private_message(const std::string& target_name, const std::string& message, ClientHandler* sender);
    void send_user_list(ClientHandler* requester);
    void send_server_notice(const std::string& message);
    void kick_user(const std::string& target_name, ClientHandler* sender = nullptr);
    void remove_client(ClientHandler* handler);
    void join_room(ClientHandler* client, std::string room_name);
    void send_room_list(ClientHandler* requester);

    ~ChatServer();
};

void createlogdir();
int logfile(std::string msg);

#endif