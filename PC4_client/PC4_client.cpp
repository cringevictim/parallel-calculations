#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <ctime>
#include <sstream>

#pragma comment(lib, "Ws2_32.lib")

#define DEFAULT_PORT "27015"
#define BUFFER_SIZE 100000 // 10000 x 10000 int матриця потребує 400MB

void PrintMatrix(const int* matrix, int size, const std::string& matrixName) {
    std::cout << matrixName << ":\n";
    for (int i = 0; i < size; ++i) {
        for (int j = 0; j < size; ++j) {
            std::cout << matrix[i * size + j] << " ";
        }
        std::cout << "\n";
    }
}

void GenerateMatrix(int* matrix, int size) {
    for (int i = 0; i < size * size; ++i) {
        matrix[i] = rand() % 100;
    }
}

void SendCommand(SOCKET connectSocket, const std::string& command) {
    send(connectSocket, command.c_str(), command.size(), 0);
}

void ReceiveResponses(SOCKET connectSocket) {
    char buffer[BUFFER_SIZE];
    while (true) {
        int recvResult = recv(connectSocket, buffer, BUFFER_SIZE, 0);
        if (recvResult <= 0) break;
        buffer[recvResult] = '\0';
        std::string response(buffer);
        std::cout << "Server response: " << response << "\n> ";
    }
    std::cout << "Connection closed." << std::endl;
}

void ProcessCommand(SOCKET connectSocket, int size, int taskId) {
    int* matrixA = new int[size * size];
    int* matrixB = new int[size * size];
    GenerateMatrix(matrixA, size);
    GenerateMatrix(matrixB, size);

    if (size <= 5) {
        PrintMatrix(matrixA, size, "Matrix A");
        PrintMatrix(matrixB, size, "Matrix B");
    }

    std::ostringstream oss;
    oss << "process " << size << " " << taskId << " ";
    for (int i = 0; i < size * size; ++i) {
        oss << matrixA[i] << " ";
    }
    for (int i = 0; i < size * size; ++i) {
        oss << matrixB[i] << " ";
    }

    SendCommand(connectSocket, oss.str());

    delete[] matrixA;
    delete[] matrixB;
}

int main() {
    WSADATA wsaData;
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        std::cerr << "WSAStartup failed: " << iResult << std::endl;
        return 1;
    }

    struct addrinfo* result = nullptr, * ptr = nullptr, hints;

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    iResult = getaddrinfo("127.0.0.1", DEFAULT_PORT, &hints, &result);
    if (iResult != 0) {
        std::cerr << "getaddrinfo failed: " << iResult << std::endl;
        WSACleanup();
        return 1;
    }

    SOCKET connectSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (connectSocket == INVALID_SOCKET) {
        std::cerr << "Error at socket(): " << WSAGetLastError() << std::endl;
        freeaddrinfo(result);
        WSACleanup();
        return 1;
    }

    iResult = connect(connectSocket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        closesocket(connectSocket);
        connectSocket = INVALID_SOCKET;
    }

    freeaddrinfo(result);

    if (connectSocket == INVALID_SOCKET) {
        std::cerr << "Unable to connect to server!" << std::endl;
        WSACleanup();
        return 1;
    }

    std::thread receiver(ReceiveResponses, connectSocket);
    receiver.detach();

    srand(time(0));
    std::string command;
    while (true) {
        std::cout << "> ";
        std::getline(std::cin, command);

        std::istringstream iss(command);
        std::string cmd;
        iss >> cmd;

        if (cmd == "process") {
            int matrixSize, taskId;
            iss >> matrixSize >> taskId;
            if (iss.fail()) {
                std::cerr << "Invalid command format. Use 'process N ID'." << std::endl;
                continue;
            }
            ProcessCommand(connectSocket, matrixSize, taskId);
        }
        else if (cmd == "status" || cmd == "result") {
            int taskId;
            iss >> taskId;
            //std::cout << taskId << std::endl;
            if (iss.fail()) {
                std::cerr << "Invalid command format. Use 'status ID' or 'result ID'." << std::endl;
                continue;
            }
            SendCommand(connectSocket, command);
        }
        else if (cmd == "exit") {
            break;
        }
        else {
            std::cout << "Unknown command. Please use 'process N ID', 'status ID', 'result ID', or 'exit'." << std::endl;
        }
    }

    iResult = shutdown(connectSocket, SD_SEND);
    if (iResult == SOCKET_ERROR) {
        std::cerr << "shutdown failed: " << WSAGetLastError() << std::endl;
    }

    closesocket(connectSocket);
    WSACleanup();

    return 0;
}
