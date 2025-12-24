#include "ChatServer_linux.h"
#include <sstream>
#include <cstring> // memset
#include <cerrno>  // errno

using namespace std;

ChatServer* g_server_instance = nullptr;

ChatServer::ChatServer(int p) : port(p), listen_socket(INVALID_SOCKET), is_running(true) {
    g_server_instance = this;
}

ChatServer::~ChatServer() {
    is_running = false;

    // 리스닝 소켓 닫기
    if (listen_socket != INVALID_SOCKET) {
        close(listen_socket); // [Linux] close
        listen_socket = INVALID_SOCKET;
    }

    // 클라이언트 핸들러 정리
    for (ClientHandler* handler : client_list) {
        delete handler;
    }
    client_list.clear();
    rooms.clear();
    // WSACleanup 제거
}

bool ChatServer::init_winsock() {
    // [Linux] WSAStartup 불필요
    return true;
}

bool ChatServer::create_listen_socket() {
    struct addrinfo* result = NULL, hints;

    // [Linux] ZeroMemory -> memset
    memset(&hints, 0, sizeof(hints));

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    int iResult = getaddrinfo(NULL, to_string(port).c_str(), &hints, &result);
    if (iResult != 0) return false;

    listen_socket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (listen_socket == INVALID_SOCKET) {
        freeaddrinfo(result);
        return false;
    }

    // [Linux 권장] 서버 재시작 시 "Address already in use" 에러 방지
    int opt = 1;
    setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    iResult = bind(listen_socket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        freeaddrinfo(result);
        close(listen_socket); // [Linux] close
        return false;
    }

    freeaddrinfo(result);
    iResult = listen(listen_socket, SOMAXCONN);
    if (iResult == SOCKET_ERROR) {
        close(listen_socket); // [Linux] close
        return false;
    }
    cout << "Server listening on port " << port << "..." << endl;
    return true;
}

void ChatServer::start_listen() {
    SOCKET client_socket = INVALID_SOCKET;
    while (is_running) {
        // accept
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        client_socket = accept(listen_socket, (struct sockaddr*)&client_addr, &client_len);

        if (!is_running) break;

        if (client_socket == INVALID_SOCKET) {
            // [Linux] 에러 확인
            // if (errno != EINTR) cerr << "accept failed" << endl;
            continue;
        }

        // IP 주소 변환
        char client_ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip_str, INET_ADDRSTRLEN);
        string client_ip(client_ip_str);

        ClientHandler* new_client = new ClientHandler(client_socket, client_ip);

        {
            lock_guard<mutex> lock(client_mutex);
            client_list.push_back(new_client);
            rooms["Lobby"].push_back(new_client);
        }
        thread client_thread(handle_client_wrapper, new_client);
        client_thread.detach();
    }
}

void ChatServer::process_admin_console() {
    string input_line;
    cout << "Server console ready. Use /notice, /kick, /list, /exit" << endl;

    while (is_running && getline(cin, input_line)) {
        if (input_line.empty()) continue;
        logfile("[Server Console] " + input_line);

        if (input_line[0] == '/') {
            stringstream ss(input_line);
            string command;
            ss >> command;

            if (command == "/notice") {
                string msg;
                getline(ss, msg);
                if (!msg.empty() && msg[0] == ' ') msg.erase(0, 1);

                if (!msg.empty()) {
                    broadcast_message(msg);
                    cout << "[Server Console] Notice sent: " << msg << endl;
                }
                else {
                    cout << "Usage: /notice <message>" << endl;
                }
            }
            else if (command == "/kick") {
                string target;
                ss >> target;
                if (!target.empty()) {
                    kick_user(target, nullptr);
                }
                else {
                    cout << "Usage: /kick <nickname>" << endl;
                }
            }
            else if (command == "/list") {
                lock_guard<mutex> lock(client_mutex);
                cout << "=== Connected Users (" << client_list.size() << ") ===" << endl;
                for (auto handler : client_list) {
                    cout << "- " << handler->getNickname() << " (" << handler->getIp() << ")" << endl;
                }
            }
            else if (command == "/exit") {
                cout << "Shutting down server..." << endl;
                is_running = false;
                close(listen_socket); // [Linux] close
                break;
            }
            else {
                cout << "Unknown command: " << command << endl;
            }
        }
        else {
            cout << "Use /notice to broadcast message." << endl;
        }
    }
}

void ChatServer::broadcast_message(const string& message, ClientHandler* sender) {
    lock_guard<mutex> lock(client_mutex);
    if (sender) {
        string room = sender->getRoom();
        if (rooms.find(room) != rooms.end()) {
            for (auto* h : rooms[room]) h->send_message(message);
        }
    }
    else {
        // 서버 공지는 전체 전송
        for (auto* h : client_list) h->send_message(message);
    }
}

void ChatServer::send_private_message(const string& target_name, const string& message, ClientHandler* sender) {
    lock_guard<mutex> lock(client_mutex);

    bool target_found = false;
    for (ClientHandler* handler : client_list) {
        if (handler->getNickname() == target_name) {
            string sender_name = (sender) ? sender->getNickname() : "Server";
            string formatted_msg = "[Whisper from " + sender_name + "]: " + message;
            handler->send_message(formatted_msg);

            if (sender) {
                sender->send_message("[Whisper to " + target_name + "]: " + message);
            }
            target_found = true;
            break;
        }
    }

    if (!target_found && sender) {
        sender->send_message("Server: User '" + target_name + "' not found.");
    }
    else if (!target_found && !sender) {
        cout << "User '" << target_name << "' not found." << endl;
    }
}

void ChatServer::send_user_list(ClientHandler* requester) {
    lock_guard<mutex> lock(client_mutex);
    string list_msg = "=== Connected Users ===\n";
    for (ClientHandler* handler : client_list) {
        list_msg += "- " + handler->getNickname() + "\n";
    }
    list_msg += "=======================";
    requester->send_message(list_msg);
}

void ChatServer::send_server_notice(const string& message) {
    lock_guard<mutex> lock(client_mutex);
    string notice_msg = "[NOTICE] " + message;
    for (ClientHandler* handler : client_list) {
        handler->send_message(notice_msg);
    }
}

void ChatServer::kick_user(const string& target_name, ClientHandler* sender) {
    ClientHandler* target_handler = nullptr;

    {
        lock_guard<mutex> lock(client_mutex);
        for (ClientHandler* handler : client_list) {
            if (handler->getNickname() == target_name) {
                target_handler = handler;
                break;
            }
        }
    }

    if (target_handler) {
        string kick_msg = "You have been kicked from the server.\n";
        // [Linux] MSG_NOSIGNAL
        send(target_handler->getSocket(), kick_msg.c_str(), (int)kick_msg.length(), MSG_NOSIGNAL);
        close(target_handler->getSocket()); // [Linux] close

        string log_msg = "Server: User '" + target_name + "' has been kicked.";
        if (sender) {
            sender->send_message(log_msg);
            cout << "[Server] " << target_name << " kicked by " << sender->getNickname() << endl;
        }
        else {
            cout << "[Server Console] Kicked user: " << target_name << endl;
        }
    }
    else {
        string err_msg = "Server: User '" + target_name + "' not found.";
        if (sender) {
            sender->send_message(err_msg);
        }
        else {
            cout << "User '" << target_name << "' not found." << endl;
        }
    }
}

void ChatServer::remove_client(ClientHandler* handler) {
    lock_guard<mutex> lock(client_mutex);
    string room = handler->getRoom();
    if (rooms.find(room) != rooms.end()) {
        auto& v = rooms[room];
        v.erase(remove(v.begin(), v.end(), handler), v.end());
    }
    client_list.erase(remove(client_list.begin(), client_list.end(), handler), client_list.end());
    cout << "[Disconnect] Client removed." << endl;
    delete handler;
}

void ChatServer::join_room(ClientHandler* client, string room_name) {
    lock_guard<mutex> lock(client_mutex);
    string old_room = client->getRoom();

    // 이전 방에서 제거
    auto& old_users = rooms[old_room];
    old_users.erase(remove(old_users.begin(), old_users.end(), client), old_users.end());
    if (old_users.empty() && old_room != "Lobby") rooms.erase(old_room);

    // 새 방 추가
    rooms[room_name].push_back(client);
    client->setRoom(room_name);

    client->send_message("Moved to room: " + room_name);
    cout << "[Room] " << client->getNickname() << " moved to " << room_name << endl;
}

void ChatServer::send_room_list(ClientHandler* req) {
    lock_guard<mutex> lock(client_mutex);
    string msg = "Rooms:\n";
    for (auto const& [name, list] : rooms) msg += "- " + name + " (" + to_string(list.size()) + ")\n";
    req->send_message(msg);
}