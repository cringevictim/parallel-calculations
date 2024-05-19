#pragma once
#include <vector>
#include <thread>
#include <random>

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