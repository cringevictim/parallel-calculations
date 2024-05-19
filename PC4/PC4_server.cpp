#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <vector>
#include <string>
#include <unordered_map>
#include <functional>
#include <sstream>
#include <algorithm>
#include <random>
#include <queue>
#include <mutex>
#include <condition_variable>



#pragma comment(lib, "Ws2_32.lib")

#define PORT "27015"
#define BUFFER_SIZE 1024

std::mutex mtx;
std::condition_variable cv;
bool running = true;

template <typename data_type>
class single_vector_matrix {
private:
    std::vector<data_type> data;
    int rows, cols;

public:
    single_vector_matrix(int r, int c) : rows(r), cols(c), data(r* c, 0) {}

    void fill_random() {
        std::generate(data.begin(), data.end(), random_number);
    }

    static data_type random_number() {
        static std::mt19937 gen{ std::random_device{}() };
        static std::uniform_int_distribution<data_type> dist(0, 100);
        return dist(gen);
    }

    void parallel_fill_random(int num_threads) {
        std::vector<std::thread> threads;
        int rows_per_thread = rows / num_threads;
        for (int i = 0; i < num_threads; ++i) {
            int start_row = i * rows_per_thread;
            int end_row = (i == num_threads - 1) ? rows : start_row + rows_per_thread;
            threads.push_back(std::thread([this, start_row, end_row]() {
                for (int r = start_row; r < end_row; ++r)
                    for (int c = 0; c < cols; ++c)
                        set_at(r, c, random_number());
                }));
        }
        for (auto& t : threads) {
            t.join();
        }
    }

    int get_rows() const { return rows; }
    int get_cols() const { return cols; }

    data_type at(int r, int c) const {
        int total_idx = r * cols + c;
        return data[total_idx];
    }

    void set_at(int r, int c, data_type value) {
        int total_idx = r * cols + c;
        data[total_idx] = value;
    }

    friend std::ostream& operator<<(std::ostream& out, const single_vector_matrix& m) {
        int total_idx = 0;
        for (int i = 0; i < m.rows; ++i) {
            for (int j = 0; j < m.cols; ++j, total_idx++) {
                out << m.data[total_idx] << " ";
            }
            out << "\n";
        }
        return out;
    }

    std::string serialize() const {
        std::ostringstream oss;
        oss << rows << " " << cols << " ";
        for (const auto& value : data) {
            oss << value << " ";
        }
        return oss.str();
    }

    static single_vector_matrix deserialize(const std::string& str) {
        std::istringstream iss(str);
        int rows, cols;
        iss >> rows >> cols;
        single_vector_matrix matrix(rows, cols);
        for (int i = 0; i < rows * cols; ++i) {
            iss >> matrix.data[i];
        }
        return matrix;
    }

    void subtract_matrix(const single_vector_matrix& other, single_vector_matrix& result, int start_row, int end_row) const {
        for (int i = start_row; i < end_row; ++i) {
            for (int j = 0; j < cols; ++j) {
                result.set_at(i, j, at(i, j) - other.at(i, j) * 2);
            }
        }
    }
};

struct Task {
    SOCKET client_socket;
    std::string matrix1_str;
    std::string matrix2_str;
};

std::queue<Task> task_queue;
std::mutex queue_mutex;

std::string process_matrices(const std::string& serialized_matrix1, const std::string& serialized_matrix2) {
    auto matrix1 = single_vector_matrix<int>::deserialize(serialized_matrix1);
    auto matrix2 = single_vector_matrix<int>::deserialize(serialized_matrix2);
    single_vector_matrix<int> result(matrix1.get_rows(), matrix1.get_cols());
    matrix1.subtract_matrix(matrix2, result, 0, matrix1.get_rows());
    std::this_thread::sleep_for(std::chrono::seconds(3));
    return result.serialize();
}

void worker_thread() {
    while (running) {
        std::unique_lock<std::mutex> lock(queue_mutex);
        cv.wait(lock, [] { return !task_queue.empty() || !running; });

        if (!running) break;

        Task task = task_queue.front();
        task_queue.pop();
        lock.unlock();

        std::string result = process_matrices(task.matrix1_str, task.matrix2_str);

        int sent = send(task.client_socket, result.c_str(), result.length(), 0);
        if (sent == SOCKET_ERROR) {
            std::cerr << "Send failed: " << WSAGetLastError() << std::endl;
        }
    }
}

void handle_client(SOCKET client_socket) {
    char buffer[BUFFER_SIZE];
    int result;

    while (true) {
        result = recv(client_socket, buffer, BUFFER_SIZE, 0);
        if (result > 0) {
            buffer[result] = '\0';
            std::string command(buffer);
            std::istringstream iss(command);
            std::string operation;
            iss >> operation;

            if (operation == "status") {
                std::lock_guard<std::mutex> lock(queue_mutex);
                std::string status = task_queue.empty() ? "No tasks in queue/Tasks are beeing processed" : std::string("Tasks in queue: ").append(std::to_string(task_queue.size()));
                send(client_socket, status.c_str(), status.length(), 0);
            }
            else if (operation == "process") {
                int N;
                iss >> N;
                single_vector_matrix<int> matrix1(N, N);
                single_vector_matrix<int> matrix2(N, N);
                matrix1.fill_random();
                matrix2.fill_random();

                Task task;
                task.client_socket = client_socket;
                task.matrix1_str = matrix1.serialize();
                task.matrix2_str = matrix2.serialize();

                {
                    std::lock_guard<std::mutex> lock(queue_mutex);
                    task_queue.push(task);
                }
                cv.notify_one();
            }
            else {
                std::string unknown_command = "unknown_command";
                send(client_socket, unknown_command.c_str(), unknown_command.length(), 0);
            }
        }
        else if (result == 0) {
            std::cout << "Connection closing..." << std::endl;
            break;
        }
        else {
            std::cerr << "recv failed: " << WSAGetLastError() << std::endl;
            break;
        }
    }

    closesocket(client_socket);
}

int main() {
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        std::cerr << "WSAStartup failed: " << result << std::endl;
        return 1;
    }

    struct addrinfo* result_list = nullptr, hints;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    result = getaddrinfo(NULL, PORT, &hints, &result_list);
    if (result != 0) {
        std::cerr << "getaddrinfo failed: " << result << std::endl;
        WSACleanup();
        return 1;
    }

    SOCKET listen_socket = socket(result_list->ai_family, result_list->ai_socktype, result_list->ai_protocol);
    if (listen_socket == INVALID_SOCKET) {
        std::cerr << "socket failed: " << WSAGetLastError() << std::endl;
        freeaddrinfo(result_list);
        WSACleanup();
        return 1;
    }

    result = bind(listen_socket, result_list->ai_addr, (int)result_list->ai_addrlen);
    if (result == SOCKET_ERROR) {
        std::cerr << "bind failed: " << WSAGetLastError() << std::endl;
        freeaddrinfo(result_list);
        closesocket(listen_socket);
        WSACleanup();
        return 1;
    }

    freeaddrinfo(result_list);

    result = listen(listen_socket, SOMAXCONN);
    if (result == SOCKET_ERROR) {
        std::cerr << "listen failed: " << WSAGetLastError() << std::endl;
        closesocket(listen_socket);
        WSACleanup();
        return 1;
    }

    std::vector<std::thread> threads;
    // For demonstartion purposes set for 2, use std::thread::hardware_concurrency instead
    for (int i = 0; i < 2; ++i) {
        threads.emplace_back(worker_thread);
    }

    while (true) {
        SOCKET client_socket = accept(listen_socket, NULL, NULL);
        if (client_socket == INVALID_SOCKET) {
            std::cerr << "accept failed: " << WSAGetLastError() << std::endl;
            closesocket(listen_socket);
            WSACleanup();
            return 1;
        }

        std::cout << "Client connected." << std::endl;
        std::thread(handle_client, client_socket).detach();
    }

    for (auto& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }

    closesocket(listen_socket);
    WSACleanup();

    return 0;
}
