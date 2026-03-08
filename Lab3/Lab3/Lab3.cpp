#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <chrono>
#include <random>
#include <atomic>
#include <iomanip>
#include <Windows.h>

using namespace std;

// Структура для збору статистики
struct Stats {
    atomic<int> total_tasks{ 0 };
    atomic<int> rejected_tasks{ 0 };
    atomic<int> completed_tasks{ 0 };
    atomic<long long> total_idle_ms{ 0 }; // Час очікування потоків
};

class CustomThreadPool {
private:
    struct Worker {
        thread t;
        function<void()> task = nullptr;
        bool busy = false;
        bool stop = false;
        mutex mtx;
        condition_variable cv;
        chrono::steady_clock::time_point idle_start;
    };

    vector<Worker*> workers;
    size_t num_threads;
    bool paused = false;
    bool global_stop = false;
    mutex pool_mtx;
    condition_variable pause_cv;
    Stats& stats;

    void worker_loop(Worker* w) {
        while (true) {
            {
                unique_lock<mutex> lock(w->mtx);
                w->idle_start = chrono::steady_clock::now();

                // Чекаємо задачу або сигнал зупинки
                w->cv.wait(lock, [w, this] {
                    return (w->task != nullptr || w->stop || global_stop) && !paused;
                    });

                if ((w->stop || global_stop) && w->task == nullptr) return;

                // Розрахунок часу очікування (idle time)
                auto idle_end = chrono::steady_clock::now();
                stats.total_idle_ms += chrono::duration_cast<chrono::milliseconds>(idle_end - w->idle_start).count();
            }

            // Виконання задачі
            if (w->task) {
                w->task();
                {
                    lock_guard<mutex> lock(w->mtx);
                    w->task = nullptr;
                    w->busy = false;
                    stats.completed_tasks++;
                }
            }
        }
    }

public:
    CustomThreadPool(size_t n, Stats& s) : num_threads(n), stats(s) {
        for (size_t i = 0; i < n; ++i) {
            Worker* w = new Worker();
            w->t = thread(&CustomThreadPool::worker_loop, this, w);
            workers.push_back(w);
        }
    }

    bool add_task(function<void()> t) {
        stats.total_tasks++;
        lock_guard<mutex> lock(pool_mtx);

        if (paused) {
            stats.rejected_tasks++;
            return false;
        }

        for (auto w : workers) {
            unique_lock<mutex> worker_lock(w->mtx);
            if (!w->busy) {
                w->task = t;
                w->busy = true;
                worker_lock.unlock();
                w->cv.notify_one();
                return true;
            }
        }

        stats.rejected_tasks++;
        return false; // Всі зайняті - задача відкинута
    }

    void set_pause(bool p) {
        paused = p;
        if (!paused) pause_cv.notify_all();
        cout << (paused ? "[Pool Paused]" : "[Pool Resumed]") << endl;
    }

    void stop(bool graceful) {
        global_stop = true;
        for (auto w : workers) {
            {
                lock_guard<mutex> lock(w->mtx);
                if (!graceful) w->task = nullptr; // Моментальна зупинка
                w->stop = true;
            }
            w->cv.notify_one();
            if (w->t.joinable()) w->t.join();
            delete w;
        }
        workers.clear();
    }

    ~CustomThreadPool() {
        if (!workers.empty()) stop(true);
    }
};

// --- ТЕСТУВАЛЬНА ЛОГІКА ---

void simulated_task(int id) {
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> dis(8, 12);
    int duration = dis(gen);

    // cout << "  -> Задача " << id << " почата (тривалість: " << duration << "с)" << endl;
    this_thread::sleep_for(chrono::seconds(duration));
    // cout << "  <- Задача " << id << " завершена" << endl;
}

int main() {
    SetConsoleCP(1251);
    SetConsoleOutputCP(1251);

    Stats stats;
    CustomThreadPool pool(6, stats);
    atomic<bool> producer_running{ true };
    atomic<int> task_id_counter{ 0 };

    // Потік-продюсер (додає задачі з декількох потоків)
    auto producer = [&](int producer_id) {
        random_device rd;
        mt19937 gen(rd());
        uniform_int_distribution<> delay(1, 3); // Нова задача кожні 1-3 сек

        while (producer_running) {
            int id = ++task_id_counter;
            bool success = pool.add_task([id] { simulated_task(id); });

            if (success)
                cout << "[Producer " << producer_id << "] Задача " << id << " додана" << endl;
            else
                cout << "[Producer " << producer_id << "] Задача " << id << " ВІДКИНУТА (всі зайняті)" << endl;

            this_thread::sleep_for(chrono::seconds(delay(gen)));
        }
        };

    cout << "Початок тестування (30 секунд)..." << endl;
    vector<thread> producers;
    for (int i = 0; i < 3; ++i) producers.emplace_back(producer, i);

    // Демонстрація паузи
    this_thread::sleep_for(chrono::seconds(5));
    pool.set_pause(true);
    this_thread::sleep_for(chrono::seconds(3));
    pool.set_pause(false);

    this_thread::sleep_for(chrono::seconds(22));
    producer_running = false;
    for (auto& p : producers) p.join();

    cout << "\nЗавершення роботи пулу (Graceful)..." << endl;
    pool.stop(true);

    // Розрахунок статистики
    cout << "\n--- СТАТИСТИКА ТЕСТУВАННЯ ---" << endl;
    cout << "Створено задач:     " << stats.total_tasks << endl;
    cout << "Відкинуто задач:    " << stats.rejected_tasks << endl;
    cout << "Виконано задач:     " << stats.completed_tasks << endl;

    double avg_idle = (stats.total_idle_ms / 6.0) / 1000.0;
    cout << "Середній час очікування потоку: " << fixed << setprecision(2) << avg_idle << " сек" << endl;

    return 0;
}