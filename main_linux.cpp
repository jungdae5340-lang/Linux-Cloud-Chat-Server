#include "ChatServer_linux.h"
#include <thread>
using namespace std;

int main() {
    createlogdir();
    logfile("Server started");
    const int PORT = 8080;

    ChatServer server(PORT);

    if (!server.init_winsock()) {
        return 1;
    }

    if (!server.create_listen_socket()) {
        return 1;
    }
    // 1. 클라이언트 접속 대기(Listening)는 별도 스레드에서 실행
    thread listen_thread(&ChatServer::start_listen, &server);
    // 2. 메인 스레드는 관리자 콘솔 입력 처리 (Blocking)
    server.process_admin_console();
    // 3. 콘솔 종료(/exit) 시 리스닝 스레드 종료 대기
    if (listen_thread.joinable()) {
        listen_thread.join();
    }

    return 0;
}