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

