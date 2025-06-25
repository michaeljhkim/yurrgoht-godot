#pragma once

#include "core/object/ref_counted.h"
#include "circular_buffer.h"
#include <atomic>
#include <deque>
#include <array>
#include <memory>

//#include "core/templates/lru.h"
#include "lru_friend.h"


/*
- CircularBuffers were used because I wanted efficient, fixed sized queues

- buffer 0 will always be the buffer to be processed
- if there is no proccessing going on, and buffer 0 has less tasks than buffer 1, we swap buffers, and raise the processing flag

- in the future, I might consider using more buffers, but for now, 2 is fine
*/
struct TaskBufferManager {
    std::atomic_bool PROCESSING = false;
    std::array<std::unique_ptr<CircularBuffer<Callable>>, 2> TASK_BUFFERS;
    
    const int MAX_BUFFER_SIZE = 100;

    // mostly for keeping the cpp code clean
    void main_thread_process() {
        if (PROCESSING.load()) {
            for (; !TASK_BUFFERS[0]->empty(); TASK_BUFFERS[0]->pop_front()) {
                TASK_BUFFERS[0]->front().call();
            }

            PROCESSING.store(false, std::memory_order_acquire);
        }
    }
    
    void tasks_push_back(Callable task) {
        TASK_BUFFERS[1]->push_back(task);

        if (!PROCESSING.load() && (TASK_BUFFERS[0]->size() < TASK_BUFFERS[1]->size())) {
            TASK_BUFFERS[0].swap(TASK_BUFFERS[1]);      // buffers being pointers makes efficient/clean swapping possible
            PROCESSING.store(true, std::memory_order_acquire);
        }
    }

    void erase_all_tasks() {
        for (int i = 0; i < TASK_BUFFERS.size(); ++i) {
            TASK_BUFFERS[i]->clear();
        }
    }

    TaskBufferManager() {
        TASK_BUFFERS[0] = std::make_unique<CircularBuffer<Callable>>(32);
        TASK_BUFFERS[1] = std::make_unique<CircularBuffer<Callable>>(32);
    }
};



/*
- LRUQueue is just a modified version of LRUCache
- LRUCache can do O(1) existance checks, inserts, and erase, but does not allow getting/popping oldest values
- LRUQueue just implements that
- lru.h was copied as lru_friend.h because I needed to make the private members protected
*/

template <typename TKey, typename TData>
class LRUQueue: public LRUCache<TKey, TData> {
	// only new function I added - does not update cache order, do not need to
    /*
	const TKey &get_back_key() {
		return (*(this->_list).back())->key;
	}
	const TData &get_back_data() {
		return (*(this->_list).back())->data;
	}
    */

public:
    //the back of the cache is the oldest values
    void pop_front() {
        this->erase((*(this->_list).back())->key);
    }

    TData front() {
        return (*(this->_list).back())->data;
    }

    bool empty() const {
        return this->get_size() <= 0;
    }
};