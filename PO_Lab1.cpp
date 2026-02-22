#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <iomanip>
#include <Windows.h>

using namespace std;


void transpose_sequential(const vector<double>& A, vector<double>& B, int n) {
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            B[j * n + i] = A[i * n + j];
        }
    }
}


void transpose_range(const vector<double>& A, vector<double>& B, int n, int start_row, int end_row) {
    for (int i = start_row; i < end_row; ++i) {
        for (int j = 0; j < n; ++j) {
            B[j * n + i] = A[i * n + j];
        }
    }
}


void transpose_parallel(const vector<double>& A, vector<double>& B, int n, int num_threads) {
    vector<thread> threads;
    int rows_per_thread = n / num_threads;

    for (int i = 0; i < num_threads; ++i) {
        int start = i * rows_per_thread;
        int end = (i == num_threads - 1) ? n : (i + 1) * rows_per_thread;
        threads.emplace_back(transpose_range, ref(A), ref(B), n, start, end);
    }

    for (auto& t : threads) t.join();
}

void run_test(int n) {
    vector<double> A(n * n, 1.0);
    vector<double> B(n * n, 0.0);

    cout << "\n--- Розмір матриці: " << n << "x" << n << " ---\n";

    // послідовно
    auto start = chrono::high_resolution_clock::now();
    transpose_sequential(A, B, n);
    auto end = chrono::high_resolution_clock::now();
    chrono::duration<double, std::milli> diff = end - start;

    // для запобігання оптимізації
    volatile double check = B[n - 1];
    cout << "Послідовно: " << diff.count() << " ms\n";

    int logical_cores = 12;
    int physical_cores = 6;
    vector<int> thread_counts = {
        physical_cores / 2,         // 3
        physical_cores,             // 6
        logical_cores,              // 12
        logical_cores * 2,          // 24
        logical_cores * 4,          // 48
        logical_cores * 8,          // 96
        logical_cores * 16          // 192
    };

    for (int tc : thread_counts) {
        start = chrono::high_resolution_clock::now();
        transpose_parallel(A, B, n, tc);
        end = chrono::high_resolution_clock::now();
        diff = end - start;
        volatile double check_p = B[0];
        cout << "Потоків: " << setw(3) << tc << " | Час: " << diff.count() << " ms\n";
    }
}

int main() {
    SetConsoleCP(1251);
    SetConsoleOutputCP(1251);

    run_test(1000);
    run_test(4000);
    run_test(8000);
    run_test(15000);
    return 0;
}
