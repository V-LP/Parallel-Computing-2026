#include <iostream>
#include <vector>
#include <thread>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")

#pragma pack(push, 1)
struct Header {
    uint8_t command;
    uint32_t length;
};
#pragma pack(pop)

bool readExact(SOCKET sock, char* buffer, size_t size) {
    size_t total_read = 0;
    while (total_read < size) {
        int bytes = recv(sock, buffer + total_read, size - total_read, 0);
        if (bytes == SOCKET_ERROR || bytes == 0) return false;
        total_read += bytes;
    }
    return true;
}

void sendHeader(SOCKET sock, uint8_t cmd, uint32_t len) {
    Header h = { cmd, htonl(len) };
    send(sock, (char*)&h, sizeof(h), 0);
}

int main() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        std::cerr << "[C++ Client] Failed to connect to server.\n";
        return 1;
    }
    std::cout << "[C++ Client] Connected to server.\n";

    uint32_t N = 3;
    uint32_t threads = 4;
    std::vector<int32_t> matrix = { 1, 2, 3, 4, 5, 6, 7, 8, 9 };

    // 1. Надсилання INIT
    uint32_t payload_len = 8 + (N * N * 4);
    sendHeader(sock, 0x01, payload_len);

    uint32_t init_data[2] = { htonl(N), htonl(threads) };
    send(sock, (char*)init_data, 8, 0);

    std::vector<int32_t> net_matrix = matrix;
    for (auto& val : net_matrix) val = htonl(val);
    send(sock, (char*)net_matrix.data(), N * N * 4, 0);

    Header resp;
    readExact(sock, (char*)&resp, sizeof(Header));
    if (resp.command == 0x10) std::cout << "[C++ Client] INIT acknowledged.\n";

    // 2. Надсилання START
    sendHeader(sock, 0x02, 0);
    readExact(sock, (char*)&resp, sizeof(Header));
    if (resp.command == 0x10) std::cout << "[C++ Client] Computations started.\n";

    // 3. Опитування статусу
    while (true) {
        sendHeader(sock, 0x03, 0);
        readExact(sock, (char*)&resp, sizeof(Header));
        resp.length = ntohl(resp.length);

        uint8_t status;
        readExact(sock, (char*)&status, 1);

        if (status == 1) {
            std::vector<int32_t> result(N * N);
            readExact(sock, (char*)result.data(), N * N * 4);

            std::cout << "[C++ Client] Transposed matrix:\n";
            for (uint32_t i = 0; i < N; ++i) {
                for (uint32_t j = 0; j < N; ++j) {
                    std::cout << ntohl(result[i * N + j]) << " ";
                }
                std::cout << "\n";
            }
            break;
        }
        else {
            std::cout << "[C++ Client] Status: processing, waiting...\n";
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    closesocket(sock);
    WSACleanup();
    return 0;
}
