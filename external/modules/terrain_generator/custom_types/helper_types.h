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
    std::array<std::unique_ptr<RingBuffer<Callable>>, 2> BUFFER;
    std::array<std::unique_ptr<LocalVector<StringName>>, 2> ID;
	Ref<CoreBind::Mutex> task_mutex;
	Ref<CoreBind::Mutex> id_mutex;
    
    const int MAX_BUFFER_SIZE = 100;

    // called on main thread
    void main_thread_process() {
        if (PROCESSING.load()) {
            while (BUFFER[0]->data_left() > 0) {
                BUFFER[0]->read().call();
            }

            id_mutex->lock();
            ID[0]->clear();
            id_mutex->unlock();

            PROCESSING.store(false, std::memory_order_acquire);
        }
    }
    
    // called on main or worker threads
    void tasks_push_back(StringName task_name, Callable task) {
        // Task name added first to prepare for existance checks ASAP
        id_mutex->lock();
        ID[1]->push_back(task_name);
        id_mutex->unlock();

        task_mutex->lock();
        if (BUFFER[1]->space_left() > 0)
            BUFFER[1]->write(task);

        if (!PROCESSING.load() && (BUFFER[0]->data_left() <= 0) && (BUFFER[1]->data_left() > 0)) {
            BUFFER[0].swap(BUFFER[1]);      // buffer being pointers makes efficient/clean swapping possible
            
            id_mutex->lock();
            ID[0].swap(ID[1]);
            id_mutex->unlock();

            PROCESSING.store(true, std::memory_order_acquire);
        }
        task_mutex->unlock();
    }

    bool task_exists(StringName task_name) {
        id_mutex->lock();
        bool exists = ID[0]->has(task_name) || ID[1]->has(task_name);
        id_mutex->unlock();

        return exists;
    }

    MainThreadManager() {
        for (int i = 0; i < 2; ++i) {
            BUFFER[i] = std::make_unique<RingBuffer<Callable>>(6);    // 2**6 = 64 
            ID[i] = std::make_unique<LocalVector<StringName>>();
            ID[i]->reserve(64);
        }

        task_mutex.instantiate();
        id_mutex.instantiate();
    }

    ~MainThreadManager() {
        for (int i = 0; i < 2; ++i) {
            BUFFER[i]->clear();
            ID[i]->clear();
        }

        id_mutex.unref();
        task_mutex.unref();
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
    LRUQueue<StringName, Callable> queue;
    Ref<CoreBind::Mutex> mutex;
    Ref<CoreBind::Thread> thread;

    std::atomic_bool RUNNING = {true};
    std::atomic_bool EMPTY = {true};
    std::atomic_bool PROCESSING = {false};
    std::atomic<uint> SIZE = 0;

    void thread_process() {
        Callable CURRENT_TASK;

        while (RUNNING) {
            if (EMPTY.load()) {
                continue;
            }
            mutex->lock();
            CURRENT_TASK = queue.front();			// tasks do exist, copy the front of the queue
            mutex->unlock();

            // all arguements are bound BEFORE being added to task queue -> no arguements needed here
            PROCESSING.store(true, std::memory_order_acquire);
            CURRENT_TASK.call();
            PROCESSING.store(false, std::memory_order_acquire);
                
            // only safe to remove AFTER call(), due to main thread checking task queue
            mutex->lock();
            queue.pop_front();
            SIZE--;
            if (queue.empty()) {    // .empty() check could be moved directly into .store(), but would require atomics to activate when not needed
                EMPTY.store(true, std::memory_order_acquire);
            }
            mutex->unlock();
        }
    }
};

struct TaskThreadManager {
    std::array<ThreadTaskQueue, 4> TASKS;
    real_t MAX_QUEUE_SIZE = 64;

    /*
    - If a task queue is not processing, add to that queue
    - If all task queues are processing, add the task to the smallest queue
        - size determined by number of tasks (should probably add weights)
    */
    void insert_task(StringName task_name, Callable task_callable) {
        int min = 0;
        for (int i = 1; i < TASKS.size(); ++i) {
            if (!TASKS[i].PROCESSING.load()) {
                min = i;
                break;
            }
            else if (TASKS[i].PROCESSING.load() && TASKS[i].SIZE < TASKS[min].SIZE) {
                min = i;
                continue;
            }
        }
        print_line("INSERT INTO THREAD: ", min);

        TASKS[min].mutex->lock();
        TASKS[min].queue.insert(task_name, task_callable);
        TASKS[min].SIZE++;

        // task queue is no longer empty -> 'wake up' task thread (thread not actually sleeping)
        TASKS[min].EMPTY.store(false, std::memory_order_acquire);
        TASKS[min].mutex->unlock();
    }

    bool has_task(StringName task_name) {
        for (int i = 0; i < TASKS.size(); ++i) {
            TASKS[i].mutex->lock();
            bool exists = TASKS[i].queue.has(task_name);
            TASKS[i].mutex->unlock();

            if (exists) { return true; }
        }
        return false;
    }

    TaskThreadManager() {
        for (int i = 0; i < TASKS.size(); ++i) {
            TASKS[i].queue.set_capacity(MAX_QUEUE_SIZE);
            TASKS[i].mutex.instantiate();
            TASKS[i].thread.instantiate();
            TASKS[i].thread->start(callable_mp(&TASKS[i], &ThreadTaskQueue::thread_process), CoreBind::Thread::PRIORITY_NORMAL);
        }
    }

    ~TaskThreadManager() {
        for (int i = 0; i < TASKS.size(); ++i) {
            TASKS[i].RUNNING.store(false, std::memory_order_acquire);
            TASKS[i].thread->wait_to_finish();
            TASKS[i].mutex.unref();
            TASKS[i].thread.unref();
        }
    }
};