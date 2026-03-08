#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <iomanip>
#include <limits>


using namespace std;

const int INF = numeric_limits<int>::max();

// послідовно
void sequential_solve(const vector<int>& data, long long& sum, int& min_val) {
    sum = 0; min_val = INF;
    for (int x : data) {
        if (x % 2 != 0) {
            sum += x;
            if (x < min_val) min_val = x;
        }
    }
}

//MUTEX
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

// CAS
void cas_solve(const vector<int>& data, int start, int end, atomic<long long>& global_sum, atomic<int>& global_min) {
    long long local_sum = 0;
    int local_min = INF;

    for (int i = start; i < end; ++i) {
        if (data[i] % 2 != 0) {
            local_sum += data[i];
            if (data[i] < local_min) local_min = data[i];
        }
    }

    // додавання 
    long long expected_sum = global_sum.load();
    while (!global_sum.compare_exchange_weak(expected_sum, expected_sum + local_sum));

    // мінімум
    int current_min = global_min.load();
    while (local_min < current_min && !global_min.compare_exchange_weak(current_min, local_min)) {
        current_min = global_min.load();
    }
}

void run_test_suite(int N) {
    vector<int> data(N);
    for (int i = 0; i < N; ++i) data[i] = rand() % 1000 + 1;

    cout << endl << "Data size: " << N << " elements" << endl;

    // послідовно 
    long long s_sum; int s_min;
    auto start_t = chrono::high_resolution_clock::now();
    sequential_solve(data, s_sum, s_min);
    auto end_t = chrono::high_resolution_clock::now();
    cout << left << setw(20) << "Sequential:" << chrono::duration<double, milli>(end_t - start_t).count() << " ms" << endl;

    int physical = 6;
    int logical = 12;
    vector<int> thread_counts = { physical / 2, physical, logical, logical * 2, logical * 4, logical * 8, logical * 16 };

    cout << "\nThreads | Mutex (ms) | CAS (ms)" << endl;
    cout << "------------------------------------" << endl;

    for (int tc : thread_counts) {
        // Mutex
        long long m_sum = 0; int m_min = INF;
        vector<thread> m_threads;
        auto start_m = chrono::high_resolution_clock::now();
        for (int i = 0; i < tc; ++i) {
            int start = i * (N / tc);
            int end = (i == tc - 1) ? N : (i + 1) * (N / tc);
            m_threads.emplace_back(mutex_solve, ref(data), start, end, ref(m_sum), ref(m_min));
        }
        for (auto& t : m_threads) t.join();
        auto end_m = chrono::high_resolution_clock::now();

        // CAS
        atomic<long long> c_sum(0);
        atomic<int> c_min(INF);
        vector<thread> c_threads;
        auto start_c = chrono::high_resolution_clock::now();
        for (int i = 0; i < tc; ++i) {
            int start = i * (N / tc);
            int end = (i == tc - 1) ? N : (i + 1) * (N / tc);
            c_threads.emplace_back(cas_solve, ref(data), start, end, ref(c_sum), ref(c_min));
        }
        for (auto& t : c_threads) t.join();
        auto end_c = chrono::high_resolution_clock::now();

        cout << setw(7) << tc << " | "
            << setw(10) << fixed << setprecision(3) << chrono::duration<double, milli>(end_m - start_m).count() << " | "
            << setw(8) << chrono::duration<double, milli>(end_c - start_c).count() << endl;
    }
}

int main() {

    run_test_suite(1000000);   // 1 млн
    run_test_suite(10000000);  // 10 млн
    run_test_suite(50000000);  // 50 млн
    run_test_suite(500000000); // 500 млн

    return 0;
}