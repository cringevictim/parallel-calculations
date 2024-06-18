#include <atomic>
#include <thread>
#include <vector>
#include <random>
#include <iostream>
#include <climits>

class Matrix {
private:
    std::vector<int> mat;
    size_t rows;
    size_t columns;
public:
    size_t getRows() { return rows; }
    size_t getColumns() { return columns; }
    size_t getSize() { return rows * columns; }

    Matrix(size_t r, size_t c) {
        rows = r;
        columns = c;
        std::random_device dev;
        std::mt19937 rng(dev());
        std::uniform_int_distribution<std::mt19937::result_type> dist6(0, 9);
        mat.resize(r * c);
        for (int i = 0; i < r * c; i++) { mat[i] = dist6(rng); }
    }

    int at_line(int i) { return mat[i]; }

    friend std::ostream& operator<<(std::ostream& os, Matrix A) {
        for (int i = 0; i < A.rows; i++) {
            for (int j = 0; j < A.columns; j++) {
                os << A.mat[i * A.columns + j] << ' ';
            }
            os << '\n';
        }
        return os;
    }
};

std::atomic<int> min(INT_MAX);

void findMinInRange(Matrix& matrix, int start, int end) {
    int local_min = INT_MAX;
    for (int i = start; i < end; i++) {
        if (matrix.at_line(i) < local_min) { local_min = matrix.at_line(i); }
    }

    bool exch_happened = false;
    while (!exch_happened) {
        int global_min = min.load();
        if (global_min > local_min) {
            exch_happened = min.compare_exchange_strong(global_min, local_min);
        }
        else {
            break;
        }
    }
}

int findMinInParallel(int num_threads, Matrix& matrix) {
    std::vector<std::thread> th;
    int size = matrix.getSize();
    int chunk_size = size / num_threads;

    for (int i = 0; i < num_threads; i++) {
        int start = i * chunk_size;
        int end;
        if (i == num_threads - 1) {
            end = size;
        }
        else {
            end = start + chunk_size;
        }
        th.emplace_back(std::thread(findMinInRange, std::ref(matrix), start, end));
    }

    for (auto& t : th) {
        t.join();
    }

    return min.load();
}

int main() {
    Matrix example(10, 10);
    std::cout << example;
    std::cout << "Min: " << findMinInParallel(10, example) << '\n';
    return 0;
}
