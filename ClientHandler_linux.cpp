#include "ChatServer_linux.h"
#include <sstream>
#include <algorithm>
#include <vector> 

using namespace std;

void handle_client_wrapper(ClientHandler* handler) {
    if (g_server_instance) {
        char buffer[1024] = { 0 };
        int bytes_received;
        SOCKET client_socket = handler->getSocket();

        char name_buffer[256] = { 0 };
        int bytes_received_name = recv(client_socket, name_buffer, sizeof(name_buffer) - 1, 0);

        if (bytes_received_name > 0) {
            name_buffer[bytes_received_name] = '\0';
            string received_name = name_buffer;

            received_name.erase(std::remove(received_name.begin(), received_name.end(), '\n'), received_name.end());
            received_name.erase(std::remove(received_name.begin(), received_name.end(), '\r'), received_name.end());

            handler->setNickname(received_name);
            cout << "Client connected: " << handler->getNickname() << endl;
            logfile("Client connected: " + handler->getNickname() + " (" + handler->getIp() + ")");
            g_server_instance->broadcast_message("Server: " + handler->getNickname() + " has joined the chat.", nullptr);
        }
        else {
            handler->setNickname("[Anonymous]");
            g_server_instance->remove_client(handler);
            return;
        }

        while (true) {
            bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);

            if (bytes_received > 0) {
                buffer[bytes_received] = '\0';
                string received_msg = buffer;

                while (!received_msg.empty() && (received_msg.back() == '\n' || received_msg.back() == '\r')) {
                    received_msg.pop_back();
                }

                if (!received_msg.empty() && received_msg[0] == '/') {
                    stringstream ss(received_msg);
                    string command;
                    ss >> command;
                    logfile("Command received from " + handler->getNickname() + ": " + command);

                    if (command == "/list") {
                        g_server_instance->send_user_list(handler);
                    }
                    else if (command == "/whisper") {
                        string target_name, msg_content;
                        ss >> target_name;
                        getline(ss, msg_content);
                        if (!msg_content.empty() && msg_content[0] == ' ') msg_content.erase(0, 1);

                        if (!target_name.empty() && !msg_content.empty()) {
                            g_server_instance->send_private_message(target_name, msg_content, handler);
                        }
                        else {
                            handler->send_message("Usage: /whisper <nickname> <message>");
                        }
                    }
                    else if (command == "/notice") {
                        string msg_content;
                        getline(ss, msg_content);
                        if (!msg_content.empty() && msg_content[0] == ' ') msg_content.erase(0, 1);
                        if (!msg_content.empty()) {
                            cout << "[DEBUG] Broadcasting Notice: " << msg_content << endl;
                            g_server_instance->broadcast_message(msg_content);
                        }
                        else {
                            handler->send_message("Usage: /notice <message>");
                        }
                    }
                    else if (command == "/kick") {
                        if (handler->getNickname() == "admin") {
                            string target_name;
                            ss >> target_name;
                            if (!target_name.empty()) {
                                cout << "[DEBUG] Admin kicking user: " << target_name << endl;
                                g_server_instance->kick_user(target_name, handler);
                            }
                            else {
                                handler->send_message("Usage: /kick <nickname>");
                                logfile("Invalid kick command usage by " + handler->getNickname());
                            }
                        }
                        else {
                            cout << "[DEBUG] Kick attempt denied for: " << handler->getNickname() << endl;
                            handler->send_message("Error: You do not have permission to use this command.");
                        }
                    }
                    else if (command == "/rooms") g_server_instance->send_room_list(handler);
                    else if (command == "/join") {
                        string room; ss >> room;
                        if (!room.empty()) g_server_instance->join_room(handler, room);
                        else handler->send_message("Usage: /join <room_name>");
                        logfile(handler->getNickname() + " joined room: " + room);
                    }
                    else if (command == "/help") {
                        handler->send_message("Available commands:\n"
                            "/list - Show connected users\n"
                            "/whisper <nickname> <message> - Send private message\n"
                            "/rooms - List available chat rooms\n"
                            "/join <room_name> - Join a chat room\n"
                            "/notice <message> - Send server notice (admin only)\n"
                            "/kick <nickname> - Kick user (admin only)\n"
                            "/help - Show this help message");
                        //logfile(handler->getNickname() + " requested help.");
                    }
                    else {
                        handler->send_message("Server: Unknown command.");
                    }
                }
                else {
                    string full_msg = "[" + handler->getNickname() + "]: " + received_msg;
                    cout << "Received: " << full_msg << endl;
                    logfile("Message from " + handler->getNickname() + ": " + received_msg);
                    g_server_instance->broadcast_message(full_msg, handler);
                }
            }
            else if (bytes_received == 0) {
                cout << "Client disconnected: " << handler->getNickname() << endl;
                logfile("Client disconnected: " + handler->getNickname() + " (" + handler->getIp() + ")");
                g_server_instance->remove_client(handler);
                break;
            }
            else {
                // Error handling
                g_server_instance->remove_client(handler);
                break;
            }
        }
    }
}