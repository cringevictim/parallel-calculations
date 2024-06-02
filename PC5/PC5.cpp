#include <iostream>
#include <winsock2.h>
#include <WS2tcpip.h>
#include <string>
#include <fstream>
#include <sstream>
#include <thread>
#include <mutex>

#pragma comment(lib, "ws2_32.lib")

#define PORT 8080

std::string readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return "";
    return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

bool endsWith(const std::string& str, const std::string& suffix) {
    return str.size() >= suffix.size() && str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::string getContentType(const std::string& path) {
    if (endsWith(path, ".html")) return "text/html";
    if (endsWith(path, ".css")) return "text/css";
    if (endsWith(path, ".ico")) return "image/x-icon";
    return "text/plain";
}

void sendResponse(SOCKET clientSocket, const std::string& response) {
    send(clientSocket, response.c_str(), response.size(), 0);
}

std::mutex mtx;
void lockedConsoleOutput(std::string str) {
    std::lock_guard<std::mutex> guard(mtx);
    std::cout << str << "\n";
}

void handleRequest(SOCKET clientSocket) {
    char buffer[4096];
    int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
    if (bytesReceived > 0) {
        buffer[bytesReceived] = '\0';
        std::istringstream request(buffer);
        std::string method, path, version;
        request >> method >> path >> version;

        std::cout << "Received request: " << method << " " << path << " " << version << std::endl;

        if (method == "GET") {
            std::string fullPath = (path == "/") ? "./index.html" : "." + path;
            std::string content = readFile(fullPath);
            std::string contentType = getContentType(fullPath);

            if (!content.empty()) {
                std::string httpResponse = "HTTP/1.1 200 OK\r\nContent-Type: " + contentType + "\r\nContent-Length: " + std::to_string(content.size()) + "\r\n\r\n" + content;
                sendResponse(clientSocket, httpResponse);
            }
            else {
                std::string httpResponse = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
                sendResponse(clientSocket, httpResponse);
            }
        }
    }

    closesocket(clientSocket);
    std::string threadResut = "Socket closed: ";
    threadResut.append(std::to_string(clientSocket));
    lockedConsoleOutput(threadResut);
}

int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed\n";
        return 1;
    }

    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed\n";
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddr.sin_port = htons(PORT);

    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Bind failed\n";
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Listen failed\n";
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Server listening on port " << PORT << "...\n";

    while (true) {
        sockaddr_in clientAddr{};
        int clientAddrLen = sizeof(clientAddr);
        SOCKET clientSocket = accept(serverSocket, (sockaddr*)&clientAddr, &clientAddrLen);
        if (clientSocket == INVALID_SOCKET) {
            std::cerr << "Accept failed\n";
            continue;
        }

        char clientAddrStr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(clientAddr.sin_addr), clientAddrStr, INET_ADDRSTRLEN);
        std::cout << "Connection accepted on socket " << clientSocket << " from " << clientAddrStr << ":" << ntohs(clientAddr.sin_port) << std::endl;

        std::thread(handleRequest, clientSocket).detach();
    }

    closesocket(serverSocket);
    WSACleanup();

    return 0;
}
