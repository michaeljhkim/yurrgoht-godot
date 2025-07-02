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
struct MainThreadManager {
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

    MainThreadManager() {
        for (int i = 0; i < 2; ++i) {
            TASK_BUFFERS[i] = std::make_unique<RingBuffer<Callable>>(6);    // 2**6 = 64 
            TASK_NAMES[i] = std::make_unique<LocalVector<StringName>>();
            TASK_NAMES[i]->reserve(64);
        }

        _task_mutex.instantiate();
        _name_mutex.instantiate();
    }

    ~MainThreadManager() {
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




class ThreadTaskQueue : public RefCounted {
	GDCLASS(ThreadTaskQueue, RefCounted);

public:
    LRUQueue<StringName, Callable> _queue;
    Ref<CoreBind::Mutex> _mutex;
    Ref<CoreBind::Thread> _thread;

    std::atomic_bool RUNNING = {true};
    std::atomic_bool EMPTY = {true};
    std::atomic_bool PROCESSING = {false};
    std::atomic<uint> SIZE = 0;

    void _thread_process() {
        Callable CURRENT_TASK;

        while (RUNNING) {
            if (EMPTY.load()) {
                continue;
            }
            _mutex->lock();
            CURRENT_TASK = _queue.front();			// tasks do exist, copy the front of the queue
            _mutex->unlock();

            // all arguements are bound BEFORE being added to task queue -> no arguements needed here
            PROCESSING.store(true, std::memory_order_acquire);
            CURRENT_TASK.call();
            PROCESSING.store(false, std::memory_order_acquire);
                
            // only safe to remove AFTER call(), due to main thread checking task queue
            _mutex->lock();
            _queue.pop_front();
            SIZE--;
            if (_queue.empty()) {    // .empty() check could be moved directly into .store(), but would require atomics to activate when not needed
                EMPTY.store(true, std::memory_order_acquire);
            }
            _mutex->unlock();
        }
    }
};

struct TaskThreadManager {
    std::array<ThreadTaskQueue, 4> TASKS;
    real_t MAX_QUEUE_SIZE = 64;

    /*
    - If all task queues are processing, add the task to the smallest queue
    - If one of the task queues is not processing, add to that queue
    */
    void insert_task(StringName task_name, Callable task_callable) {
        int min = 0;
        for (int i = 1; i < TASKS.size(); ++i) {
            if (TASKS[i].PROCESSING && TASKS[i].SIZE < TASKS[min].SIZE) {
                min = i;
                continue;
            }
            else if (!TASKS[i].PROCESSING) {
                min = i;
                break;
            }
        }

        TASKS[min]._mutex->lock();
        TASKS[min]._queue.insert(task_name, task_callable);
        TASKS[min].SIZE++;

        // task queue is no longer empty -> 'wake up' task thread (thread not actually sleeping)
        TASKS[min].EMPTY.store(false, std::memory_order_acquire);
        TASKS[min]._mutex->unlock();
    }

    bool has_task(StringName task_name) {
        for (int i = 0; i < TASKS.size(); ++i) {
            TASKS[i]._mutex->lock();
            bool exists = TASKS[i]._queue.has(task_name);
            TASKS[i]._mutex->unlock();

            if (exists) { return true; }
        }
        return false;
    }

    TaskThreadManager() {
        for (int i = 0; i < TASKS.size(); ++i) {
            TASKS[i]._queue.set_capacity(64);
            TASKS[i]._mutex.instantiate();
            TASKS[i]._thread.instantiate();
            TASKS[i]._thread->start(callable_mp(&TASKS[i], &ThreadTaskQueue::_thread_process), CoreBind::Thread::PRIORITY_NORMAL);
        }
    }

    ~TaskThreadManager() {
        for (int i = 0; i < TASKS.size(); ++i) {
            TASKS[i].RUNNING.store(false, std::memory_order_acquire);
            TASKS[i]._thread->wait_to_finish();
            TASKS[i]._mutex.unref();
            TASKS[i]._thread.unref();
        }
    }
};