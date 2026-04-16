#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <winsock2.h>
#include <ws2tcpip.h>

// Вказуємо лінкеру підключити бібліотеку WinSock
#pragma comment(lib, "Ws2_32.lib")

// Структури протоколу (вирівнювання по 1 байту, щоб уникнути padding-у)
#pragma pack(push, 1)
struct Header {
    uint8_t command;
    uint32_t length;
};
#pragma pack(pop)

const uint8_t CMD_INIT = 0x01;
const uint8_t CMD_START = 0x02;
const uint8_t CMD_STATUS_REQ = 0x03;
const uint8_t CMD_ACK = 0x10;
const uint8_t CMD_RESULT = 0x11;

// Гарантоване читання N байтів
bool readExact(SOCKET sock, char* buffer, size_t size) {
    size_t total_read = 0;
    while (total_read < size) {
        int bytes = recv(sock, buffer + total_read, size - total_read, 0);
        if (bytes == SOCKET_ERROR || bytes == 0) return false;
        total_read += bytes;
    }
    return true;
}

// Відправка заголовка
void sendHeader(SOCKET sock, uint8_t cmd, uint32_t len) {
    Header h;
    h.command = cmd;
    h.length = htonl(len); // Конвертація в Network Byte Order (Big-Endian)
    send(sock, (char*)&h, sizeof(h), 0);
}

void handleClient(SOCKET client_sock) {
    std::vector<int32_t> matrix;
    uint32_t N = 0;
    bool is_done = false;

    try {
        while (true) {
            Header h;
            if (!readExact(client_sock, (char*)&h, sizeof(Header))) break;
            h.length = ntohl(h.length); // З мережевого у локальний порядок

            if (h.command == CMD_INIT) {
                uint32_t init_data[2];
                readExact(client_sock, (char*)init_data, 8);
                N = ntohl(init_data[0]);
                uint32_t threads_cnt = ntohl(init_data[1]);

                matrix.resize(N * N);
                readExact(client_sock, (char*)matrix.data(), N * N * sizeof(int32_t));

                // Десеріалізація матриці
                for (auto& val : matrix) val = ntohl(val);

                std::cout << "[Server] Отримано матрицю " << N << "x" << N << " для " << threads_cnt << " потоків.\n";
                sendHeader(client_sock, CMD_ACK, 0);

            }
            else if (h.command == CMD_START) {
                std::cout << "[Server] Початок обчислень...\n";
                // ІМІТАЦІЯ ТРАНСПОНУВАННЯ (тут має бути виклик Thread Pool)
                std::vector<int32_t> transposed(N * N);
                for (uint32_t i = 0; i < N; ++i) {
                    for (uint32_t j = 0; j < N; ++j) {
                        transposed[j * N + i] = matrix[i * N + j];
                    }
                }
                matrix = transposed;
                std::this_thread::sleep_for(std::chrono::seconds(3)); // Імітація роботи
                is_done = true;

                sendHeader(client_sock, CMD_ACK, 0);

            }
            else if (h.command == CMD_STATUS_REQ) {
                if (is_done) {
                    sendHeader(client_sock, CMD_RESULT, 1 + N * N * 4);
                    uint8_t status = 1;
                    send(client_sock, (char*)&status, 1, 0);

                    std::vector<int32_t> net_matrix = matrix;
                    for (auto& val : net_matrix) val = htonl(val); // У мережевий порядок
                    send(client_sock, (char*)net_matrix.data(), N * N * 4, 0);
                    is_done = false; // Скидаємо стан для можливих наступних команд
                }
                else {
                    sendHeader(client_sock, CMD_RESULT, 1);
                    uint8_t status = 0;
                    send(client_sock, (char*)&status, 1, 0);
                }
            }
        }
    }
    catch (...) {
        std::cerr << "[Server] Помилка обробки клієнта.\n";
    }

    std::cout << "[Server] Клієнт відключений.\n";
    closesocket(client_sock);
}

int main() {
    // Ініціалізація WinSock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "Помилка WSAStartup\n";
        return 1;
    }

    SOCKET server_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_sock == INVALID_SOCKET) {
        std::cerr << "Помилка створення сокета\n";
        WSACleanup();
        return 1;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr));
    listen(server_sock, SOMAXCONN);
    std::cout << "Сервер (Windows) слухає на порту 8080...\n";

    while (true) {
        SOCKET client_sock = accept(server_sock, nullptr, nullptr);
        if (client_sock != INVALID_SOCKET) {
            std::cout << "[Server] Нове підключення!\n";
            std::thread(handleClient, client_sock).detach();
        }
    }

    closesocket(server_sock);
    WSACleanup();
    return 0;
}
