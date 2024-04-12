#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <functional>
#include <queue>
#include <array>
#include <shared_mutex>

std::mutex g_queue_mutex;

struct Task {
private:
    int task_id;
    int waiting_time = rand() % 7 + 6;
public:
    Task(int id) : task_id(id) {}

    void run() {
        std::cout << "Started t";
    }
};

class Queue
{
private:
    int max_size;
    std::queue<std::function<void()>> queue_;
public:
    Queue(int m_size) : max_size(m_size) {}
    int size();
    bool push(std::function<void()>& task);
    std::function<void()> pop();
};

int Queue::size() {
    std::unique_lock<std::mutex> lock(g_queue_mutex);
    return this->queue_.size();
}

bool Queue::push(std::function<void()>& task) {
    std::unique_lock<std::mutex> lock(g_queue_mutex);
    if (this->size() < 10) {
        this->queue_.push(task);
        return true;
    }
    else { return false; }
}

std::function<void()> Queue::pop() {
    std::unique_lock<std::mutex> lock(g_queue_mutex);
    if (this->size() <= 0) { return false; }
    std::function<void()> task = this->queue_.front();
    this->queue_.pop();
    return task;
}

class ThreadPool
{
private:
    int num_threads_;
    int num_threads_per_queue_;
    int num_queues_;
    std::vector<Queue> queues_;
    std::vector<std::thread> threads_;

    void execute();

public:
    ThreadPool(int threads, int threads_per_queue, int queues) : num_threads_(threads), num_threads_per_queue_(threads_per_queue), num_queues_(queues) {}

    void create();
    void pause();
    void start();
    void stop();
    void add();
};

void ThreadPool::execute() {
    for (;;) {
        std::function<void()> task = this->queues_[0].pop();
        task();

    }
}

void ThreadPool::create() {
    for (int i = 0; i < this->num_threads_; i++) {
        this->threads_.push_back(std::thread());
    }
}




int main()
{
    std::cout << "Hello World!\n";
}

