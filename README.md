# ☁️ Linux Chat Server (GCP Deployed)

이 프로젝트는 기존 **Windows(Winsock2)** 기반의 채팅 서버를 **Linux(POSIX Socket)** 환경으로 완벽하게 **포팅(Porting)**하고, **Google Cloud Platform(GCP)** Compute Engine에 배포하여 실제 운용 테스트를 마친 결과물입니다.

## 📸 Deployment Proof (GCP 연동 성공)
![GCP Connection Test]("C:\Work\Linux-Cloud-Chat-Server\Final_Linux.png")
*(위: GCP 리눅스 서버 / 아래: 로컬 리눅스 클라이언트 통신 성공 화면)*

---

## 🛠️ Technical Challenges & Solutions (포팅 과정)

Windows 환경의 코드를 Linux로 변환하며 운영체제 간의 네트워크 처리 차이를 극복했습니다.

### 1. Winsock -> POSIX Socket 변환
- **라이브러리 교체:** `<winsock2.h>`를 제거하고 `<sys/socket.h>`, `<netinet/in.h>`, `<arpa/inet.h>` 등 리눅스 표준 헤더로 변경했습니다.
- **API 매핑:** - `closesocket()` ➔ `close()`
    - `SOCKET` 타입 ➔ `int`
    - `INVALID_SOCKET` ➔ `-1`
- **초기화 제거:** 윈도우 전용인 `WSAStartup()` 및 `WSACleanup()` 로직을 제거하여 리눅스 데몬 형태에 맞췄습니다.

### 2. Multi-threading & Signal Handling
- **스레드:** `std::thread`와 `pthread` 라이브러리를 사용하여 리눅스 환경에서도 멀티스레드 처리가 가능하도록 구현했습니다.
- **SIGPIPE 방지:** 리눅스 소켓 통신 중 클라이언트가 갑자기 끊길 때 발생하는 `SIGPIPE` 시그널로 인해 서버가 죽는 현상을 막기 위해, `send()` 함수에 `MSG_NOSIGNAL` 플래그를 추가했습니다.

### 3. Cloud Deployment (GCP)
- Google Cloud Platform의 **VM 인스턴스(Ubuntu)**를 구축하고 방화벽 규칙(Firewall Rules)을 설정하여 외부 포트 개방 및 통신을 구현했습니다.

---

### 🔗 4단계: 기존 저장소와 연결 (크로스 링크)

마지막으로, 아까 만든 **윈도우 서버 저장소(Original Repo)**의 README 맨 윗부분에 이 리눅스 버전을 홍보해줘.

> **📢 [New!] Linux Port & Cloud Version**
> 이 서버를 리눅스로 포팅하여 Google Cloud에 배포한 버전은 [여기(https://github.com/jungdae5340-lang/Linux-Cloud-Chat-Server/tree/main)]에서 확인할 수 있습니다.

## 🚀 How to Build (컴파일 방법)

리눅스 환경(Ubuntu/CentOS)에서 아래 명령어로 빌드할 수 있습니다.

```bash
# Server Build
g++ -o server main_linux.cpp ChatServer_linux.cpp ClientHandler_linux.cpp Logger_linux.cpp -lpthread

# Client Build
g++ -o client ChatClient_linux.cpp -lpthread
