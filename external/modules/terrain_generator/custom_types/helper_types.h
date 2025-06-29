#pragma once

#include "core/object/ref_counted.h"
#include "core/templates/ring_buffer.h"
#include <atomic>
#include <deque>
#include <array>
#include <memory>

//#include "core/templates/lru.h"
#include "lru_friend.h"


/*
- RingBuffer were used because I wanted efficient, fixed sized queues

- buffer 0 will always be the buffer to be processed
- if there is no proccessing going on, and buffer 0 has less tasks than buffer 1, we swap buffers, and raise the processing flag

- in the future, I might consider using more buffers, but for now, 2 is fine
*/
struct TaskBufferManager {
    std::atomic_bool PROCESSING = false;
    std::array<std::unique_ptr<RingBuffer<Callable>>, 2> TASK_BUFFERS;
    std::array<std::unique_ptr<LocalVector<String>>, 2> TASK_NAMES;
	Ref<CoreBind::Mutex> _task_mutex;
	Ref<CoreBind::Mutex> _name_mutex;
    
    const int MAX_BUFFER_SIZE = 100;

    // mostly for keeping the cpp code clean
    void main_thread_process() {
        if (PROCESSING.load()) {
            while (TASK_BUFFERS[0]->data_left() > 0) {
                TASK_BUFFERS[0]->read().call();
            }
            TASK_BUFFERS[0]->clear();

            _name_mutex->lock();
            TASK_NAMES[0]->clear();
            _name_mutex->unlock();

            PROCESSING.store(false, std::memory_order_acquire);
        }
    }
    
    /*
    - multiple tasks might be being pushed back at a time, so mutexes are needed
    */
    void tasks_push_back(String task_name, Callable task) {
        // Task name is added first so that it is available for existance checks ASAP
        _name_mutex->lock();
        TASK_NAMES[1]->push_back(task_name);
        _name_mutex->unlock();

        _task_mutex->lock();
        if (TASK_BUFFERS[1]->space_left() > 0) {
            TASK_BUFFERS[1]->write(task);
        }

        if (!PROCESSING.load() && (TASK_BUFFERS[0]->data_left() <= 0) && (TASK_BUFFERS[1]->data_left() > 0)) {
            TASK_BUFFERS[0].swap(TASK_BUFFERS[1]);      // buffers being pointers makes efficient/clean swapping possible
            
            _name_mutex->lock();
            TASK_NAMES[0].swap(TASK_NAMES[1]);
            _name_mutex->unlock();

            PROCESSING.store(true, std::memory_order_acquire);
        }
        _task_mutex->unlock();
    }

    bool task_exists(String task_name) {
        _name_mutex->lock();
        bool exists = TASK_NAMES[0]->has(task_name) || TASK_NAMES[1]->has(task_name);
        _name_mutex->unlock();

        return exists;
    }

    void erase_all_tasks() {
        for (int i = 0; i < TASK_BUFFERS.size(); ++i) {
            TASK_BUFFERS[i]->clear();
        }
    }

    TaskBufferManager() {
        for (int i = 0; i < 2; ++i) {
            TASK_BUFFERS[i] = std::make_unique<RingBuffer<Callable>>(6);    // 2**6 = 64 
            TASK_NAMES[i] = std::make_unique<LocalVector<String>>();
            TASK_NAMES[i]->reserve(64);
        }

        _task_mutex.instantiate();
        _name_mutex.instantiate();
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

    bool is_front(TKey key_check) {
        return key_check != (*(this->_list).back())->key;
    }

    bool empty() const {
        return this->get_size() <= 0;
    }
};