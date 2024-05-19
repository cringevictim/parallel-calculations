#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <sstream>
#include <random>
#include <vector>
#include <thread>
#include <algorithm>

#include "matrix.h"

#pragma comment(lib, "Ws2_32.lib")

#define SERVER_ADDRESS "127.0.0.1"
#define PORT "27015"
#define BUFFER_SIZE 1024



void connect_and_send(SOCKET& connect_socket) {
    struct addrinfo* result_list = nullptr, hints;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    int result = getaddrinfo(SERVER_ADDRESS, PORT, &hints, &result_list);
    if (result != 0) {
        std::cerr << "getaddrinfo failed: " << result << std::endl;
        WSACleanup();
        exit(1);
    }

    connect_socket = socket(result_list->ai_family, result_list->ai_socktype, result_list->ai_protocol);
    if (connect_socket == INVALID_SOCKET) {
        std::cerr << "socket failed: " << WSAGetLastError() << std::endl;
        freeaddrinfo(result_list);
        WSACleanup();
        exit(1);
    }

    result = connect(connect_socket, result_list->ai_addr, (int)result_list->ai_addrlen);
    if (result == SOCKET_ERROR) {
        std::cerr << "connect failed: " << WSAGetLastError() << std::endl;
        closesocket(connect_socket);
        freeaddrinfo(result_list);
        WSACleanup();
        exit(1);
    }

    freeaddrinfo(result_list);
}

void handle_response(SOCKET& connect_socket) {
    char buffer[BUFFER_SIZE];
    while (true) {
        int bytes_received = recv(connect_socket, buffer, BUFFER_SIZE, 0);
        if (bytes_received > 0) {
            buffer[bytes_received] = '\0';
            std::cout << "\nResponse: " << buffer << std::endl;
            std::cout << "Enter command (process N / status / exit): ";
        }
        else if (bytes_received == 0) {
            std::cout << "Connection closed by server." << std::endl;
            closesocket(connect_socket);
            connect_and_send(connect_socket);
        }
        else {
            std::cerr << "recv failed: " << WSAGetLastError() << std::endl;
            closesocket(connect_socket);
            connect_and_send(connect_socket);
        }
    }
}

int main() {
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        std::cerr << "WSAStartup failed: " << result << std::endl;
        return 1;
    }

    SOCKET connect_socket;
    connect_and_send(connect_socket);

    std::thread(handle_response, std::ref(connect_socket)).detach();

    std::string command;
    int N;

    while (true) {
        std::cout << "Enter command (process N / status / exit): ";
        std::getline(std::cin, command);

        if (command == "exit") {
            break;
        }
        else if (command.find("process") == 0) {
            std::istringstream iss(command);
            std::string cmd;
            std::string n_str;
            iss >> cmd >> n_str;

            if (!std::all_of(n_str.begin(), n_str.end(), ::isdigit)) {
                std::cerr << "Invalid command: N must be a number." << std::endl;
                continue;
            }

            int N = std::stoi(n_str);



            std::ostringstream oss;
            oss << "process " << N;
            int sent = send(connect_socket, oss.str().c_str(), oss.str().length(), 0);
            if (sent == SOCKET_ERROR) {
                std::cerr << "Send failed: " << WSAGetLastError() << std::endl;
            }
        }
        else if (command == "status") {
            int sent = send(connect_socket, command.c_str(), command.length(), 0);
            if (sent == SOCKET_ERROR) {
                std::cerr << "Send failed: " << WSAGetLastError() << std::endl;
            }
        }
        else {
            std::cout << "Unknown command." << std::endl;
        }
    }

    closesocket(connect_socket);
    WSACleanup();

    return 0;
}
