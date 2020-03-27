//
// Created by Aleksey Dorofeev on 25/03/2020.
//

#pragma once

#include <iostream>
#include <sstream>
#include <mutex>
#include <thread>
#include <array>
#include <chrono>

// Защищенная в плане многопоточного доступа очередь типа FIFO с предварительным резервированием.
// Организована по типу кольцевого буфера.
// В плане использования предполагается несколько потоков-писателей и несколько потоков-читателей.
template <typename Task, size_t N>
class Fifo {
public:
    enum Ret_code {
        eGood = 0,
        eInterrupted
    };

    Fifo() : ix_read(0), ix_write(0), cur_size(0)
    {
    }

    // Команда прекращения работы, вынуждает прервать обслуживание заявок из буфера и пробудить все ждущие потоки.
    void stop(std::vector<Task> &non_performed_tasks)
    {
        std::cout << "INTERRUPT" << std::endl;
        stopped_flag = true;
        cv_free.notify_all();
        cv.notify_all();

        std::this_thread::yield();

        std::unique_lock lock(mt);
        while(ix_read != ix_write) {
            non_performed_tasks.push_back(queue[ix_read]);
            if(++ix_read == queue.max_size())
                ix_read = 0;
        }
        cur_size = 0;
    }

    // Положить задачу в хвост очереди. Если очередь заполнена, поток будет заблокирован до появления свободного места.
    // Ожидающий поток может быть разблокирован по внешнему сигналу прерывания работы, в этом случае задача не кладется
    // в очередь и возвращается код eInterrupted.
    Ret_code push_back(Task const& request)
    {
        std::unique_lock lock(mt);

        bool wait = cur_size == queue.max_size();
        auto ts = std::chrono::steady_clock::now();
        while(cur_size == queue.max_size()) {
            lock.unlock();
            {
                std::unique_lock lock_free(mt_free);
                cv_free.wait(lock_free, [&] { return cur_size != queue.max_size() || stopped_flag; });
                if(stopped_flag)
                    return eInterrupted;
            }
            lock.lock();
        }
        if(wait) {
            auto te = std::chrono::steady_clock::now();
            auto dt = std::chrono::duration_cast<std::chrono::microseconds>(te - ts);
            wait_for_free_space_us += dt;
        }

        if(stopped_flag)
            return eInterrupted;

        std::cout << "PUSH" << ix_write << std::endl;
        queue[ix_write] = request;
        if(++ix_write == queue.max_size())
            ix_write = 0;
        ++cur_size;

        ++push_tasks;

        if(cur_size == 1) {
            std::cout << "NOTIFY";
            cv.notify_one();
        }

        return eGood;
    }

    // Взять задачу из головы очереди. Если очередь пуста, поток будет заблокирован до появления задачи.
    // Ожидающий поток может быть разблокирован по внешнему сигналу, в этом случае задача не кладется в очередь и
    // возвращается код eInterrupted.
    Ret_code pop_front(Task &result)
    {
        std::unique_lock lock(mt);

        bool wait = cur_size == 0;
        auto ts = std::chrono::steady_clock::now();
        while(cur_size == 0) {
            cv.wait(lock, [&] { return cur_size || stopped_flag; });
            if(stopped_flag)
                return eInterrupted;
        }
        if(wait) {
            auto te = std::chrono::steady_clock::now();
            auto dt = std::chrono::duration_cast<std::chrono::microseconds>(te - ts);
            wait_for_task_us += dt;
        }

        if(stopped_flag)
            return eInterrupted;

        std::cout << "POP" << ix_read;

        ++pop_tasks;

        result = queue[ix_read];
        if(++ix_read == queue.max_size())
            ix_read = 0;
        if(cur_size-- == queue.max_size())
            cv_free.notify_one();
        return eGood;
    }

public:
    inline static thread_local std::chrono::microseconds wait_for_free_space_us{0};
    inline static thread_local uint64_t push_tasks{0};

    inline static thread_local std::chrono::microseconds wait_for_task_us{0};
    inline static thread_local uint64_t pop_tasks{0};

private:
    std::atomic_bool stopped_flag;

    std::mutex mt;
    std::condition_variable cv;

    std::mutex mt_free;
    std::condition_variable cv_free;

    std::array<Task, N> queue;
    size_t ix_read;
    size_t ix_write;
    size_t cur_size;
};

