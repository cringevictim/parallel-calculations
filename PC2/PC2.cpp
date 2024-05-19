#include "thread_pool.h"
#include <iostream>
#include <random>
#include "global.h"

//#define TRACY_ENABLE

//#include "tracy/Tracy.hpp"


void sampleTask(int id, int sleeping_time) {
    std::this_thread::sleep_for(std::chrono::seconds(sleeping_time));

    g_console_lock.lock();
    std::cout << "Task " << id << " was executed on thread " << std::this_thread::get_id() << " and lasted " << sleeping_time << " seconds." << std::endl;
    g_console_lock.unlock();
}

int main() {
    //ZoneScoped;
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
