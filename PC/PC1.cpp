#include <iostream>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <thread>
#include <chrono>
#include <algorithm>

int RandomNumber() {
    return std::rand() % 10;
}

template <typename DataType>
class SingleVectorMatrix {
private:
    std::vector<DataType> data;
    int rows, cols;

public:
    SingleVectorMatrix(int r, int c) : rows(r), cols(c), data(r* c, 0) {}

    void fillRandom() {
        std::generate(data.begin(), data.end(), RandomNumber);
    }

    int getRows() const { return rows; }
    int getCols() const { return cols; }

    DataType at(int r, int c) const {
        int total_idx = r * cols + c;
        return data[total_idx];
    }

    void setAt(int r, int c, DataType value) {
        int total_idx = r * cols + c;
        data[total_idx] = value;
    }

    friend std::ostream& operator<<(std::ostream& out, const SingleVectorMatrix& m) {
        int total_idx = 0;
        for (int i = 0; i < m.rows; ++i) {
            for (int j = 0; j < m.cols; ++j, total_idx++) {
                out << m.data[total_idx] << " ";
            }
            out << "\n";
        }
        return out;
    }

    void subtractMatrix(const SingleVectorMatrix& other, SingleVectorMatrix& result, int startRow, int endRow) { // перевірити вираз на правильність
        for (int i = startRow; i < endRow; ++i) {
            for (int j = 0; j < cols; ++j) {
                result.setAt(i, j, at(i, j) - other.at(i, j));
            }
        }
    }
};

void parallelSubtract(SingleVectorMatrix<int>& A, SingleVectorMatrix<int>& B, SingleVectorMatrix<int>& C, int numThreads) {
    std::vector<std::thread> threads;
    int rowsPerThread = A.getRows() / numThreads;

    for (int i = 0; i < numThreads; ++i) {
        int startRow = i * rowsPerThread;
        int endRow = (i + 1) * rowsPerThread;

        if (i == numThreads - 1) {
            endRow = A.getRows();
        }

        threads.push_back(std::thread(&SingleVectorMatrix<int>::subtractMatrix, &A, std::ref(B), std::ref(C), startRow, endRow));
    }

    for (auto& t : threads) {
        t.join();
    }
}

int main() {
    srand(time(0));
    SingleVectorMatrix<int> A(15000, 15000), B(15000, 15000), C(15000, 15000);
    A.fillRandom();
    B.fillRandom();

    int numThreads = 2048;
    std::cout << "Num Threads: " << numThreads << std::endl;
    auto start = std::chrono::high_resolution_clock::now();
    parallelSubtract(A, B, C, numThreads);
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    std::cout << "Time elapsed for parallel subtract: " << elapsed.count() << " s\n";

    numThreads = 32;
    std::cout << "Num Threads: " << numThreads << std::endl;
    start = std::chrono::high_resolution_clock::now();
    parallelSubtract(A, B, C, numThreads);
    end = std::chrono::high_resolution_clock::now();
    elapsed = end - start;
    std::cout << "Time elapsed for parallel subtract: " << elapsed.count() << " s\n";

    numThreads = 16;
    std::cout << "Num Threads: " << numThreads << std::endl;
    start = std::chrono::high_resolution_clock::now();
    parallelSubtract(A, B, C, numThreads);
    end = std::chrono::high_resolution_clock::now();
    elapsed = end - start;
    std::cout << "Time elapsed for parallel subtract: " << elapsed.count() << " s\n";

    numThreads = 8;
    std::cout << "Num Threads: " << numThreads << std::endl;
    start = std::chrono::high_resolution_clock::now();
    parallelSubtract(A, B, C, numThreads);
    end = std::chrono::high_resolution_clock::now();
    elapsed = end - start;
    std::cout << "Time elapsed for parallel subtract: " << elapsed.count() << " s\n";

    numThreads = 4;
    std::cout << "Num Threads: " << numThreads << std::endl;
    start = std::chrono::high_resolution_clock::now();
    parallelSubtract(A, B, C, numThreads);
    end = std::chrono::high_resolution_clock::now();
    elapsed = end - start;
    std::cout << "Time elapsed for parallel subtract: " << elapsed.count() << " s\n";

    numThreads = 2;
    std::cout << "Num Threads: " << numThreads << std::endl;
    start = std::chrono::high_resolution_clock::now();
    parallelSubtract(A, B, C, numThreads);
    end = std::chrono::high_resolution_clock::now();
    elapsed = end - start;
    std::cout << "Time elapsed for parallel subtract: " << elapsed.count() << " s\n";

    numThreads = 1;
    std::cout << "Num Threads: " << numThreads << std::endl;
    start = std::chrono::high_resolution_clock::now();
    parallelSubtract(A, B, C, numThreads);
    end = std::chrono::high_resolution_clock::now();
    elapsed = end - start;
    std::cout << "Time elapsed for parallel subtract: " << elapsed.count() << " s\n";


    return 0;
}
