#pragma once

#include "core/object/ref_counted.h"
#include "core/templates/rb_set.h"
#include <atomic>
#include <deque>
#include <array>
#include <unordered_set>


struct TaskBufferManager {
    std::atomic_bool PROCESSING[2] = {false, false};
    std::atomic_bool READY[2] = {false, false};
    std::deque<Callable> TASK_BUFFERS[2];
    const int MAX_BUFFER_SIZE = 100;

    int index = 0;

    void index_increment() {
        ++index;

        if (index >= TASK_BUFFERS->size()) 
            index = 0;
    }

    // mostly for keeping the cpp code clean
    void main_thread_process(uint index) {
        if (READY[index].load()) {
		    READY[index].exchange(false, std::memory_order_acquire);
            PROCESSING[index].exchange(true, std::memory_order_acquire);

            /*
            for (; !TASK_BUFFERS[index].empty(); TASK_BUFFERS[index].pop_front()) {
                TASK_BUFFERS[index].front().call();
            }
            */
            for (const Callable &task : TASK_BUFFERS[index]) {
                task.call();
            }
            TASK_BUFFERS[index].clear();

            PROCESSING[index].exchange(false, std::memory_order_acquire);
            //index_increment();
        }
    }
    
    void tasks_push_back(Callable task) {
        if (!PROCESSING[0].load()) {
            TASK_BUFFERS[0].push_back(task); 

            if (!TASK_BUFFERS[0].empty())
                READY[0].exchange(true, std::memory_order_acquire);
            
            print_line(0, TASK_BUFFERS[0].size());
        }
        else if (!PROCESSING[1].load()) {
            TASK_BUFFERS[1].push_back(task);

            if (!TASK_BUFFERS[1].empty())
                READY[1].exchange(true, std::memory_order_acquire);
            
            print_line(1, TASK_BUFFERS[1].size());
        }
    }

    void erase_all_tasks() {
        for (int i = 0; i < TASK_BUFFERS->size(); ++i) {
            TASK_BUFFERS[i].clear();
        }
    }

    TaskBufferManager() {}
};




struct CallableDeque {
    Vector<Array> queue;
    Callable callback;

    bool contains(Array pargs) {
        return queue.has(pargs);
    }

    void set_callback(Callable new_callback) {
        callback = new_callback;
    }

    bool push_back(Array call_binds) {
        queue.push_back(call_binds);
        return true;
    }

    void pop_front() {
        queue.remove_at(0);
    }

    Callable front() {
        return callback.bindv(queue[0]);
    }

    bool empty() const {
        return queue.is_empty();
    }

    void clear() {
        queue.clear();
        callback.~Callable();
    }
};