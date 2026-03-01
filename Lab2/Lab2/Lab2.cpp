#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <algorithm>
#include <limits>

using namespace std;

const int INF = numeric_limits<int>::max();

// 1. ПОСЛІДОВНЕ ВИКОНАННЯ
void sequential_solve(const vector<int>& data, long long& sum, int& min_val) {
    sum = 0; min_val = INF;
    for (int x : data) {
        if (x % 2 != 0) {
            sum += x;
            if (x < min_val) min_val = x;
        }
    }
}

// 2. БЛОКУЮЧА СИНХРОНІЗАЦІЯ (MUTEX)
mutex mtx;
void mutex_solve(const vector<int>& data, int start, int end, long long& global_sum, int& global_min) {
    long long local_sum = 0;
    int local_min = INF;

    for (int i = start; i < end; ++i) {
        if (data[i] % 2 != 0) {
            local_sum += data[i];
            if (data[i] < local_min) local_min = data[i];
        }
    }

    // Критична секція
    lock_guard<mutex> lock(mtx);
    global_sum += local_sum;
    if (local_min < global_min) global_min = local_min;
}

// 3. АТОМАРНІ ОПЕРАЦІЇ ТА CAS (БЕЗ fetch_add)
void cas_solve(const vector<int>& data, int start, int end, atomic<long long>& global_sum, atomic<int>& global_min) {
    for (int i = start; i < end; ++i) {
        if (data[i] % 2 != 0) {
            int val = data[i];

            // Ручна реалізація атомарного додавання через CAS
            long long expected_sum = global_sum.load();
            while (!global_sum.compare_exchange_weak(expected_sum, expected_sum + val));

            // Ручна реалізація атомарного мінімуму через CAS
            int current_min = global_min.load();
            while (val < current_min && !global_min.compare_exchange_weak(current_min, val)) {
                current_min = global_min.load(); // Оновити поточний мінімум, якщо CAS не вдався
            }
        }
    }
}
