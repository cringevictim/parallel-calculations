#include <atomic>
#include <thread>
#include <vector>
#include <random>
#include <iostream>
#include <climits>

class Matrix { // клас матриці
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
        std::uniform_int_distribution<std::mt19937::result_type> dist6(0, 9); // повертає випадкові числа в межах від 0 до 9. Взяв десь на stackoverflow, якщо чесно
        mat.resize(r * c);
        for (int i = 0; i < r * c; i++) { mat[i] = dist6(rng); } // заповнюємо матрицю
    }

    int at_line(int i) { return mat[i]; }

    friend std::ostream& operator<<(std::ostream& os, Matrix A) { // перевантажуємо оператор виводу для зручного відображення в консолі
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

void findMinInRange(Matrix& matrix, int start, int end) { // функція для пошуку найменшого елементу в діапазоні
    int local_min = INT_MAX;
    for (int i = start; i < end; i++) {
        if (matrix.at_line(i) < local_min) { local_min = matrix.at_line(i); } // спочатку зберігаємо значення мінімума в діапазоні локально, щоб не звертатися до атоміка зайвий раз
    }

    bool exch_happened = false; // тут прям щось накрутив, але воно працює...
    while (!exch_happened) {
        int global_min = min.load();
        if (global_min > local_min) { // завантажуємо глобальний мінімум, порівнюємо з локальним
            exch_happened = min.compare_exchange_strong(global_min, local_min); // якщо порівняння проходить, то виконуємо заміну по експектед-дезайред і визодимо з циклу у разі успіху 
        }
        else {
            break; // якщо мінмум і так менше, ніж в діапазоні, виходимо з циклу без заміни
        }
    }
}

int findMinInParallel(int num_threads, Matrix& matrix) {
    std::vector<std::thread> th;
    int size = matrix.getSize();
    int chunk_size = size / num_threads; // визначаємо розмір ділянки до обробки в потоці

    for (int i = 0; i < num_threads; i++) {
        int start = i * chunk_size;
        int end;
        if (i == num_threads - 1) {
            end = size;
        }
        else {
            end = start + chunk_size;
        }
        th.emplace_back(std::thread(findMinInRange, std::ref(matrix), start, end)); // створюємо потік на обробку ділянки
    }

    for (auto& t : th) {
        t.join();
    }

    return min.load(); // повертаємо глобальний мінімум (атомік) після завершення роботи всіх потоків
}

int main() {
    Matrix example(10, 10);
    std::cout << example;
    std::cout << "Min: " << findMinInParallel(10, example) << '\n';
    return 0;
}
