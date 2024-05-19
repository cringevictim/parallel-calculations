#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <thread>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <sstream>

#pragma comment(lib, "Ws2_32.lib")

#define DEFAULT_PORT "27015"
#define BUFFER_SIZE 100000 // 10000 x 10000 int req ~400MB

enum TaskStatus { PENDING, COMPLETED };

struct Task {
    std::string result;
    int size;
    TaskStatus status;
};

std::unordered_map<SOCKET, std::unordered_map<int, Task>> clientTasks;
std::mutex taskMutex;
std::condition_variable taskCv;
bool serverRunning = true;
void dbg_count() {
    static int num = 1;
    std::cout << num << std::endl;
    num++;
}

void PrintMatrix(const int* matrix, int size, const std::string& matrixName) {
    std::cout << matrixName << ":\n";
    for (int i = 0; i < size; ++i) {
        for (int j = 0; j < size; ++j) {
            std::cout << matrix[i * size + j] << " ";
        }
        std::cout << "\n";
    }
}

void HandleClient(SOCKET clientSocket) {
    std::cout << "Client connected: " << clientSocket << std::endl;

    while (serverRunning) {
        char buffer[BUFFER_SIZE];
        int recvResult = recv(clientSocket, buffer, BUFFER_SIZE-1, 0);
        //std::cout << recvResult << std::endl;
        if (recvResult <= 0) break;
        buffer[recvResult] = '\0';
        std::string command(buffer);
        std::istringstream iss(command);
        std::string cmd;
        iss >> cmd;
        if (cmd == "process") {
            int matrixSize, taskId;
            iss >> matrixSize >> taskId;
            std::string matrixData;
            std::getline(iss, matrixData);

            {
                std::unique_lock<std::mutex> lock(taskMutex);
                clientTasks[clientSocket][taskId] = Task{ "", matrixSize, PENDING };
            }
            std::cout << "dbg" << std::endl;
            std::thread([clientSocket, matrixSize, taskId, matrixData]() {
                std::istringstream matrixStream(matrixData);
                int* matrixA = new int[matrixSize * matrixSize];
                int* matrixB = new int[matrixSize * matrixSize];

                for (int i = 0; i < matrixSize * matrixSize; ++i) {
                    matrixStream >> matrixA[i];
                }
                for (int i = 0; i < matrixSize * matrixSize; ++i) {
                    matrixStream >> matrixB[i];
                }

                if (matrixSize <= 5) {
                    PrintMatrix(matrixA, matrixSize, "Matrix A");
                    PrintMatrix(matrixB, matrixSize, "Matrix B");
                }

                int* result = new int[matrixSize * matrixSize];
                std::ostringstream resultStream;
                for (int i = 0; i < matrixSize * matrixSize; ++i) {
                    result[i] = matrixA[i] - matrixB[i];
                    resultStream << result[i] << " ";
                }

                if (matrixSize <= 5) {
                    PrintMatrix(result, matrixSize, "Result Matrix");
                }

                {
                    std::unique_lock<std::mutex> lock(taskMutex);
                    clientTasks[clientSocket][taskId].result = resultStream.str();
                    clientTasks[clientSocket][taskId].status = COMPLETED;
                }
                taskCv.notify_all();

                delete[] matrixA;
                delete[] matrixB;
                delete[] result;

                send(clientSocket, "completed", 9, 0);
                }).detach();
        }
        else if (cmd == "status") {
            int taskId;
            iss >> taskId;
            std::unique_lock<std::mutex> lock(taskMutex);
            if (clientTasks[clientSocket].find(taskId) != clientTasks[clientSocket].end()) {
                std::string status = clientTasks[clientSocket][taskId].status == PENDING ? "pending" : "completed";
                send(clientSocket, status.c_str(), status.size(), 0);
            }
            else {
                send(clientSocket, "unknown", 7, 0);
            }
        }
        else if (cmd == "result") {
            int taskId;
            iss >> taskId;
            std::unique_lock<std::mutex> lock(taskMutex);
            if (clientTasks[clientSocket].find(taskId) != clientTasks[clientSocket].end() && clientTasks[clientSocket][taskId].status == COMPLETED) {
                send(clientSocket, clientTasks[clientSocket][taskId].result.c_str(), clientTasks[clientSocket][taskId].result.size(), 0);
            }
            else if (clientTasks[clientSocket].find(taskId) != clientTasks[clientSocket].end()) {
                send(clientSocket, "pending", 7, 0);
            }
            else {
                send(clientSocket, "unknown", 7, 0);
            }
        }
        else if (cmd == "shutdown") {
            serverRunning = false;
            break;
        }
    }

    {
        std::unique_lock<std::mutex> lock(taskMutex);
        clientTasks.erase(clientSocket);
    }

    closesocket(clientSocket);
    std::cout << "Client disconnected: " << clientSocket << std::endl;
}

int main() {
    WSADATA wsaData;
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        std::cerr << "WSAStartup failed: " << iResult << std::endl;
        return 1;
    }

    struct addrinfo* result = nullptr, hints;

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
    if (iResult != 0) {
        std::cerr << "getaddrinfo failed: " << iResult << std::endl;
        WSACleanup();
        return 1;
    }

    SOCKET listenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (listenSocket == INVALID_SOCKET) {
        std::cerr << "Error at socket(): " << WSAGetLastError() << std::endl;
        freeaddrinfo(result);
        WSACleanup();
        return 1;
    }

    iResult = bind(listenSocket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        std::cerr << "bind failed with error: " << WSAGetLastError() << std::endl;
        freeaddrinfo(result);
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    freeaddrinfo(result);

    if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Listen failed with error: " << WSAGetLastError() << std::endl;
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Server is listening on port " << DEFAULT_PORT << std::endl;

    while (serverRunning) {
        SOCKET clientSocket = accept(listenSocket, NULL, NULL);
        if (clientSocket == INVALID_SOCKET) {
            std::cerr << "accept failed: " << WSAGetLastError() << std::endl;
            closesocket(listenSocket);
            WSACleanup();
            return 1;
        }
        std::thread(HandleClient, clientSocket).detach();
    }

    closesocket(listenSocket);
    WSACleanup();

    return 0;
}
