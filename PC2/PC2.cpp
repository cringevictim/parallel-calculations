#include "thread_pool.h"
#include <iostream>
#include <random>
#include "global.h"

void sampleTask(int id, int sleeping_time) {
    std::this_thread::sleep_for(std::chrono::seconds(sleeping_time));

    g_console_lock.lock();
    std::cout << "Task " << id << " executed on thread " << std::this_thread::get_id() << " and lasted " << sleeping_time << " seconds." << std::endl;
    g_console_lock.unlock();
}

int main() {
    thread_pool pool;

    pool.initialize(2, 3, 10);

    for (int i = 0; i < 40; ++i) {
        int sleeping_time = rand() % 11;
        pool.add_task(sampleTask, i, sleeping_time);
    }

    std::this_thread::sleep_for(std::chrono::seconds(5));
    pool.terminate();

    return 0;
}
