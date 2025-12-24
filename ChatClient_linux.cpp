#include <iostream>
#include <string>
#include <thread>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <atomic> // [필수] 스레드 간 안전한 변수 공유를 위해 추가

using namespace std;

// [설정] 서버 IP와 포트 (GCP 외부 IP로 변경 필요할 수 있음)
const char* PORT = "8080";
const char* SERVER_IP = "34.158.199.128";

// --- [UI] ANSI 색상 코드 (Linux는 기본 지원) ---
#define RESET   "\033[0m"
#define RED     "\033[31m"      // 시스템/오류
#define GREEN   "\033[32m"      // 나 (My Chat)
#define YELLOW  "\033[33m"      // 공지/명령어
#define BLUE    "\033[34m"      // 정보/로고
#define MAGENTA "\033[35m"      // 귓속말
#define CYAN    "\033[36m"      // 상대방
#define BOLD    "\033[1m"       // 굵게
#define DIM     "\033[2m"       // 흐리게
atomic<bool> connection_active(true);
// [UI] 로고 출력 함수
void print_logo() {
    // 리눅스 터미널 클리어 명령
    cout << "\033[2J\033[1;1H";

    cout << BOLD CYAN;
    cout << "===================================================" << endl;
    cout << R"(
  ____ _           _     ____                  
 / ___| |__   __ _| |_  / ___|___  _ __ ___    
| |   | '_ \ / _` | __|| |   / _ \| '__/ _ \   
| |___| | | | (_| | |_ | |__| (_) | | |  __/   
 \____|_| |_|\__,_|\__| \____\___/|_|  \___|   
                                               
      [ TCP/IP Multi-Thread Chat Core ]        
)" << endl;
    cout << "===================================================" << endl;
    cout << RESET << endl;
}

// 메시지 수신 스레드
void receive_msg(int sock) {
    char buf[1024];
    while (true) {
        int len = recv(sock, buf, 1024, 0);
        // 출력 라인 정리 (입력 중인 줄 덮어쓰기 방지)
        cout << "\r" << string(60, ' ') << "\r";
        if (len > 0) {
            buf[len] = '\0';
            string msg = buf;

            if (msg.find("You have been kicked") != string::npos) {
                cout << RED << "⚠️ " << msg << RESET << endl;
                cout << RED << "서버에 의해 접속이 강제 종료됩니다. . . " << RESET << endl;
                exit(0); // 프로그램 전체를 즉시 종료합니다.
            }
            // 메시지 종류별 색상 분기
            if (msg.find("[NOTICE]") != string::npos) {
                cout << BOLD YELLOW << "📢 " << msg << RESET << endl;
            }
            else if (msg.find("[Whisper]") != string::npos) {
                cout << MAGENTA << "🔒 " << msg << RESET << endl;
            }
            else if (msg.find("joined") != string::npos || msg.find("left") != string::npos) {
                cout << DIM << BLUE << "ℹ️  " << msg << RESET << endl;
            }
            else if (msg.find("Server:") != string::npos || msg.find("Kicked") != string::npos) {
                cout << RED << "⚠️ " << msg << RESET << endl;
            }
            else {
                // 일반 채팅
                cout << CYAN << msg << RESET << endl;
            }
        }
        else if (len == 0) {
            cout << RED << "Connection closed by server." << RESET << endl;
            break;
        }
        else {
            cerr << RED << "recv failed with error: " << RESET << endl;
            break;
        }
        // 입력 프롬프트 복구 (초록색)
        cout << GREEN << "Me > " << RESET << flush;
    }
    connection_active.store(false);
}

int main() {
    // 리눅스는 WSAStartup, SetConsoleMode 불필요 (기본 지원)
    print_logo();

    struct addrinfo hints, * result;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    if (getaddrinfo(SERVER_IP, PORT, &hints, &result) != 0) {
        cout << RED << "주소 해석 실패!" << RESET << endl;
        return 1;
    }

    int sock = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (sock < 0) {
        cout << RED << "소켓 생성 실패!" << RESET << endl;
        return 1;
    }

    if (connect(sock, result->ai_addr, result->ai_addrlen) < 0) {
        cout << RED << "서버 연결 실패! (서버가 켜져 있는지 확인하세요)" << RESET << endl;
        close(sock);
        return 1;
    }

    freeaddrinfo(result);

    cout << BLUE << "서버 연결 성공! 닉네임을 입력하세요: " << RESET;

    // 닉네임 입력 (초록색)
    string nick;
    cout << GREEN;
    getline(cin, nick);
    cout << RESET;

    // 리눅스에서 send 시 MSG_NOSIGNAL 옵션 권장 (연결 끊김 시 시그널 방지)
    send(sock, nick.c_str(), (int)nick.length(), MSG_NOSIGNAL);

    // 안내 메시지
    cout << "---------------------------------------------------" << endl;
    cout << " " << YELLOW << "/help 를 사용하여 명령어 사용법을 확인할 수 있습니다!" << RESET << endl;
    cout << "---------------------------------------------------" << endl;

    thread(receive_msg, sock).detach();

    string line;
    while (connection_active.load()) {
        // 내 입력 프롬프트 (초록색)
        cout << GREEN << "Me > " << RESET;

        if (!getline(cin, line)) break;
        if (!connection_active.load() || line == "exit") break;

        send(sock, line.c_str(), (int)line.length(), MSG_NOSIGNAL);
    }
    connection_active.store(false);
    close(sock);
    return 0;
}