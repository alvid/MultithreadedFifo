#include <iostream>
#include <thread>
#include <vector>
#include <numeric>
#include <random>
#include <functional>

#include "Fifo.hpp"

enum {
    FIFO_SIZE = 32,
    WORKER_THREAD_COUNT = 2,
    EXECUTOR_THREAD_COUNT = 2,
    ELEM_COUNT = 10,
    RUN_COUNT = 32
};

using Task = std::function<int ()>;
using Fifo_type = Fifo<Task, FIFO_SIZE>;

int sum(std::vector<int> numbers)
{
    return std::accumulate(numbers.begin(), numbers.end(), 0);
}

void worker_routine(Fifo_type &fifo, size_t elem_count, size_t run_count)
{
    std::random_device rd;
    std::mt19937 g(rd());

    for(size_t i=0; i<run_count; ++i) {
        std::vector<int> numbers;
        for (size_t j=0; j<elem_count; ++j)
            numbers.push_back(g());
        auto res = fifo.push_back(std::bind(&sum, std::move(numbers)));
        if (res == Fifo_type::eInterrupted)
            break;
        //std::this_thread::sleep_for(std::chrono::nanoseconds(200));
    }
}

void executor_routine(Fifo_type &fifo)
{
    Fifo_type::Ret_code res;
    Task task;
    while((res = fifo.pop_front(task)) != Fifo_type::eInterrupted) {
        if(task)
            int rc = task();
        // эмулируем тяжелую работу
        std::this_thread::sleep_for(std::chrono::nanoseconds(1000));
    }
}

int main()
{
    std::cout << "Hello, asynchronous World!" << std::endl;

    Fifo_type fifo;

    std::vector<std::thread> threads;

    for(size_t i = 0; i < EXECUTOR_THREAD_COUNT; ++i)
        threads.emplace_back(&executor_routine, std::ref(fifo));

    for(size_t i = 0; i < WORKER_THREAD_COUNT; ++i)
        threads.emplace_back(&worker_routine, std::ref(fifo), ELEM_COUNT, RUN_COUNT);

    std::this_thread::sleep_for(std::chrono::seconds(2));
    std::vector<Task> nft;
    fifo.stop(nft);

    for(auto &item: threads)
        item.join();

    return 0;
}
