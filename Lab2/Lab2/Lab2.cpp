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

//послідовно
void sequential_solve(const vector<int>& data, long long& sum, int& min_val) {
    sum = 0; min_val = INF;
    for (int x : data) {
        if (x % 2 != 0) {
            sum += x;
            if (x < min_val) min_val = x;
        }
    }
}

//з блокуючою синхронізацією
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

    lock_guard<mutex> lock(mtx);
    global_sum += local_sum;
    if (local_min < global_min) global_min = local_min;
}

//атомарні операції
void cas_solve(const vector<int>& data, int start, int end, atomic<long long>& global_sum, atomic<int>& global_min) {
    for (int i = start; i < end; ++i) {
        if (data[i] % 2 != 0) {
            int val = data[i];

            
            long long expected_sum = global_sum.load();
            while (!global_sum.compare_exchange_weak(expected_sum, expected_sum + val));

            
            int current_min = global_min.load();
            while (val < current_min && !global_min.compare_exchange_weak(current_min, val)) {
                current_min = global_min.load();
            }
        }
    }
}

void run_benchmarks(int N, int num_threads) {
    vector<int> data(N);
    for (int i = 0; i < N; ++i) data[i] = rand() % 1000 + 1;

    cout << "\n>>> Тест: N = " << N << ", Потоків = " << num_threads << endl;

    // послідовно
    long long s_sum; int s_min;
    auto start = chrono::high_resolution_clock::now();
    sequential_solve(data, s_sum, s_min);
    auto end = chrono::high_resolution_clock::now();
    cout << "Sequential: " << chrono::duration<double, milli>(end - start).count() << " ms | Sum: " << s_sum << endl;

    // MUTEX
    long long m_sum = 0; int m_min = INF;
    vector<thread> m_threads;
    start = chrono::high_resolution_clock::now();
    for (int i = 0; i < num_threads; ++i) {
        m_threads.emplace_back(mutex_solve, ref(data), i * (N / num_threads), (i + 1) * (N / num_threads), ref(m_sum), ref(m_min));
    }
    for (auto& t : m_threads) t.join();
    end = chrono::high_resolution_clock::now();
    cout << "Mutex:      " << chrono::duration<double, milli>(end - start).count() << " ms | Sum: " << m_sum << endl;

    // CAS 
    atomic<long long> c_sum(0);
    atomic<int> c_min(INF);
    vector<thread> c_threads;
    start = chrono::high_resolution_clock::now();
    for (int i = 0; i < num_threads; ++i) {
        c_threads.emplace_back(cas_solve, ref(data), i * (N / num_threads), (i + 1) * (N / num_threads), ref(c_sum), ref(c_min));
    }
    for (auto& t : c_threads) t.join();
    end = chrono::high_resolution_clock::now();
    cout << "CAS (CMPXCHG): " << chrono::duration<double, milli>(end - start).count() << " ms | Sum: " << c_sum.load() << endl;
}

int main() {
    int physical_cores = 6;
    for (int size : {100000, 1000000, 10000000}) {
        run_benchmarks(size, physical_cores);
    }
    return 0;
}