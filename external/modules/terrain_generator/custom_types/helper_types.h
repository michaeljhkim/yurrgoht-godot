#pragma once

#include "core/object/ref_counted.h"
#include "core/templates/ring_buffer.h"
#include <atomic>
#include <array>
#include <memory>

//#include "core/templates/lru.h"
#include "lru_friend.h"


/*
- RingBuffer is used because efficient fixed-sized queues are needed

- buffer 0 -> process buffer
- buffer 1 -> storage buffer 
- if no proccessing is occurring, swap buffer data, and raise processing flag

- in the future, consider using more buffers, but 2 is fine for now
*/
struct TaskBufferManager {
    std::atomic_bool PROCESSING = false;
    std::array<std::unique_ptr<RingBuffer<Callable>>, 2> TASK_BUFFERS;
    std::array<std::unique_ptr<LocalVector<StringName>>, 2> TASK_NAMES;
	Ref<CoreBind::Mutex> _task_mutex;
	Ref<CoreBind::Mutex> _name_mutex;
    
    const int MAX_BUFFER_SIZE = 100;

    // called on main thread
    void main_thread_process() {
        if (PROCESSING.load()) {
            while (TASK_BUFFERS[0]->data_left() > 0) {
                TASK_BUFFERS[0]->read().call();
            }

            _name_mutex->lock();
            TASK_NAMES[0]->clear();
            _name_mutex->unlock();

            PROCESSING.store(false, std::memory_order_acquire);
        }
    }
    
    // called on main or worker threads
    void tasks_push_back(StringName task_name, Callable task) {
        // Task name added first to prepare for existance checks ASAP
        _name_mutex->lock();
        TASK_NAMES[1]->push_back(task_name);
        _name_mutex->unlock();

        _task_mutex->lock();
        if (TASK_BUFFERS[1]->space_left() > 0)
            TASK_BUFFERS[1]->write(task);

        if (!PROCESSING.load() && (TASK_BUFFERS[0]->data_left() <= 0) && (TASK_BUFFERS[1]->data_left() > 0)) {
            TASK_BUFFERS[0].swap(TASK_BUFFERS[1]);      // buffers being pointers makes efficient/clean swapping possible
            
            _name_mutex->lock();
            TASK_NAMES[0].swap(TASK_NAMES[1]);
            _name_mutex->unlock();

            PROCESSING.store(true, std::memory_order_acquire);
        }
        _task_mutex->unlock();
    }

    bool task_exists(StringName task_name) {
        _name_mutex->lock();
        bool exists = TASK_NAMES[0]->has(task_name) || TASK_NAMES[1]->has(task_name);
        _name_mutex->unlock();

        return exists;
    }

    TaskBufferManager() {
        for (int i = 0; i < 2; ++i) {
            TASK_BUFFERS[i] = std::make_unique<RingBuffer<Callable>>(6);    // 2**6 = 64 
            TASK_NAMES[i] = std::make_unique<LocalVector<StringName>>();
            TASK_NAMES[i]->reserve(64);
        }

        _task_mutex.instantiate();
        _name_mutex.instantiate();
    }

    ~TaskBufferManager() {
        for (int i = 0; i < 2; ++i) {
            TASK_BUFFERS[i]->clear();
            TASK_NAMES[i]->clear();
        }

        _name_mutex.unref();
        _task_mutex.unref();
    }
};



/*
- LRUQueue is a slightly modified version of LRUCache
- LRUCache can do O(1) existance checks, inserts, and erase, but does not allow getting/popping oldest values (unless they are known)
- LRUQueue merely implements that
- lru.h was copied as lru_friend.h -> only modifications were private members are now protected
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