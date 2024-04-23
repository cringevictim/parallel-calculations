#include <iostream>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <thread>
#include <chrono>
#include <algorithm>
#include <fstream>

inline int random_number() {
    return std::rand() % 10;
}

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

    void subtract_matrix(const single_vector_matrix& other, single_vector_matrix& result, int start_row, int end_row) {
        for (int i = start_row; i < end_row; ++i) {
            for (int j = 0; j < cols; ++j) {
                result.set_at(i, j, at(i, j) - other.at(i, j) * 2);
            }
        }
    }
};

void parallel_subtract(single_vector_matrix<int>& A, single_vector_matrix<int>& B, single_vector_matrix<int>& C, int num_threads) {
    std::vector<std::thread> threads;
    int rows_per_thread = A.get_rows() / num_threads;

    for (int i = 0; i < num_threads; ++i) {
        int start_row = i * rows_per_thread;
        int end_row = (i + 1) * rows_per_thread;

        if (i == num_threads - 1) {
            end_row = A.get_rows();
        }

        threads.push_back(std::thread(&single_vector_matrix<int>::subtract_matrix, &A, std::ref(B), std::ref(C), start_row, end_row));
    }

    for (auto& t : threads) {
        t.join();
    }
}

void start_task(int num_threads, int size, std::ofstream& log_file) {
    single_vector_matrix<int> A(size, size), B(size, size), C(size, size);
    auto start = std::chrono::high_resolution_clock::now();
    A.parallel_fill_random(num_threads);
    B.parallel_fill_random(num_threads);
    parallel_subtract(A, B, C, num_threads);
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    std::cout << "Threads: [" << num_threads << "]; Time elapsed: " << elapsed.count() << " s." << std::endl;
    log_file << num_threads << "," << elapsed.count() << "\n";
}

int main() {
    srand(time(0));
    int size = 20000;
    std::ofstream log_file("performance_data.csv");
    log_file << "Threads,Time\n";

    std::vector<int> thread_counts = { 1, 4, 8, 16, 24, 32, 48, 64, 96, 128, 192, 256, 384 };

    for (int threads : thread_counts) {
        start_task(threads, size, log_file);
    }

    log_file.close();
    return 0;
}









