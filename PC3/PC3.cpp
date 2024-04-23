#include <iostream>
#include <chrono>
#include <atomic>
#include <random>
#include <thread>
#include <vector>
#include <mutex>
#include <limits>

std::vector<int> g_values;
std::mutex g_mutex;


void fillVectorWithRandom(std::vector<int>& vec, int min, int max, int count) {
	std::mt19937 rng(time(nullptr));
	std::uniform_int_distribution<int> dist(min, max);
	vec.clear();
	vec.reserve(count);
	for (int i = 0; i < count; i++) {
		vec.push_back(dist(rng));
	}
}


void single_thread(int N, int &above_n_amount, int &max) {
	above_n_amount = 0;
	max = INT_MIN;
	for (auto num : g_values) {
		if (num > N) { above_n_amount++; }
		if (num > max) { max = num; }
	}
}

void locked_multithread(int* start, int* end, int& above_n_amount, int& max, int N) {
	for (int* ptr = start; ptr != end; ptr++) {
		if (*ptr > N) { 
            g_mutex.lock();
            above_n_amount++;
            g_mutex.unlock();
        }
		if (*ptr > max) {
            g_mutex.lock();
            max = *ptr;
            g_mutex.unlock();
        }
	}
}

void atomic_multithread(int* start, int* end, std::atomic<int>& above_n_amount, std::atomic<int>& max, int N) {
    for (int* ptr = start; ptr != end; ptr++) {
		if (*ptr > N) {
			above_n_amount.fetch_add(1, std::memory_order_relaxed);
		}
		for (;;) {
			int current_max = max.load(std::memory_order_relaxed);
			if (*ptr <= current_max || max.compare_exchange_weak(current_max, *ptr, std::memory_order_relaxed)) break;
		}
	}
}

int main() {
    int above_n_amount = 0;
    int max = 0;
    int N = 10;

    fillVectorWithRandom(g_values, 0, 32, 10000000);

    auto start_time = std::chrono::high_resolution_clock::now();
    single_thread(N, above_n_amount, max);
    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end_time - start_time;
    std::cout << "[Single thread] " << "Above N amount: " << above_n_amount << "; Maximum: " << max << "; Elapsed time: " << elapsed.count() << " ms" << std::endl;

    above_n_amount = 0;
    max = 0;

    start_time = std::chrono::high_resolution_clock::now();
    size_t locked_threads_amount = std::thread::hardware_concurrency();
    std::vector<std::thread> workers;
    size_t locked_chunk_size = g_values.size() / locked_threads_amount;
    for (size_t i = 0; i < locked_threads_amount; i++) {
        int* start = &g_values[i * locked_chunk_size];
        int* end = (i == locked_threads_amount - 1) ? &g_values.back() + 1 : start + locked_chunk_size;
        workers.push_back(std::thread(locked_multithread, start, end, std::ref(above_n_amount), std::ref(max), N));
    }
    for (auto& worker : workers) {
        worker.join();
    }
    end_time = std::chrono::high_resolution_clock::now();
    elapsed = end_time - start_time;
    std::cout << "[Locked multithreading] " << "Above N amount: " << above_n_amount << "; Maximum: " << max << "; Elapsed time: " << elapsed.count() << " ms" << std::endl;

    std::atomic<int> atomic_above_n_amount(0);
    std::atomic<int> atomic_max(INT_MIN);

    start_time = std::chrono::high_resolution_clock::now();
    size_t atomic_threads_amount = std::thread::hardware_concurrency();
    std::vector<std::thread> atomic_workers;
    size_t atomic_chunk_size = g_values.size() / atomic_threads_amount;
    for (size_t i = 0; i < atomic_threads_amount; i++) {
        int* start = &g_values[i * atomic_chunk_size];
        int* end = (i == atomic_threads_amount - 1) ? &g_values.back() + 1 : start + atomic_chunk_size;
        atomic_workers.push_back(std::thread(atomic_multithread, start, end, std::ref(atomic_above_n_amount), std::ref(atomic_max), N));
    }
    for (auto& worker : atomic_workers) {
        worker.join();
    }
    end_time = std::chrono::high_resolution_clock::now();
    elapsed = end_time - start_time;
    std::cout << "[Atomic multithreading] " << "Above N amount: " << atomic_above_n_amount << "; Maximum: " << atomic_max << "; Elapsed time: " << elapsed.count() << " ms" << std::endl;

    return 0;
}