#pragma once

#include "core/object/ref_counted.h"
#include "core/templates/ring_buffer.h"
#include <atomic>
#include <array>
#include <memory>

//#include "core/templates/lru.h"
#include "lru_friend.h"

#include "core/os/mutex.h"

// purely for keeping code cleaner
/*
if flip is false, incrementing iteration
if flip is true, decrementing iteration
*/
struct range_flip {
    range_flip(int min, int max, bool flip = false): last(max), iter(min) {
        if (flip) {
            iter = max;
            last = min;
            step = -1;
        }
    }

    // Iterable functions
    _FORCE_INLINE_ const range_flip& begin() const { return *this; }
    _FORCE_INLINE_ const range_flip& end() const { return *this; }

    // Iterator functions
    _FORCE_INLINE_ bool operator==(const range_flip&) const { return iter == last; }
    _FORCE_INLINE_ bool operator!=(const range_flip&) const { return iter != last; }
    _FORCE_INLINE_ void operator++() { iter += step; }
    _FORCE_INLINE_ int operator*() const { return iter; }

private:
    int last;
    int iter;
    int step = 1;
};

/*
- RingBuffer is used because efficient fixed-sized queues are needed

- buffer 0 -> process buffer
- buffer 1 -> storage buffer 
- if no proccessing is occurring, swap buffer data, and raise processing flag

- Seperate Vector was used for task ID's, so that existance checks can be done safely while a buffer is being processed

- in the future, consider using more buffers, but 2 is fine for now
*/
struct MainThreadManager {
    std::atomic_bool PROCESSING = false;
    std::array<std::unique_ptr<RingBuffer<Callable>>, 2> BUFFER;
    std::array<std::unique_ptr<LocalVector<StringName>>, 2> ID;
	Mutex task_mutex;
	Mutex id_mutex;
    
    const int MAX_BUFFER_SIZE = 100;

    // called on main thread
    void process_tasks() {
        if (PROCESSING.load()) {
            while (BUFFER[0]->data_left() > 0) {
                BUFFER[0]->read().call();
            }

            {
                MutexLock mutex_lock(id_mutex);
                ID[0]->clear();
            }

            PROCESSING.store(false, std::memory_order_acquire);
        }
    }
    
    // called on main or worker threads
    void tasks_push_back(StringName task_name, Callable task) {
        // Task name added first to prepare for existance checks ASAP
        {
            MutexLock mutex_lock(id_mutex);
            ID[1]->push_back(task_name);
        }
        
		MutexLock lock1(task_mutex);
        if (BUFFER[1]->space_left() > 0)
            BUFFER[1]->write(task);

        if (!PROCESSING.load() && (BUFFER[0]->data_left() <= 0) && (BUFFER[1]->data_left() > 0)) {
            BUFFER[0].swap(BUFFER[1]);      // buffer being pointers makes efficient/clean swapping possible
            {
                MutexLock mutex_lock(id_mutex);
                ID[0].swap(ID[1]);
            }
            PROCESSING.store(true, std::memory_order_acquire);
        }
    }

    bool task_exists(StringName task_name) {
        MutexLock mutex_lock(id_mutex);
        return ID[0]->has(task_name) || ID[1]->has(task_name);
    }

    MainThreadManager() {
        for (int i = 0; i < 2; ++i) {
            BUFFER[i] = std::make_unique<RingBuffer<Callable>>(6);    // 2**6 = 64 
            ID[i] = std::make_unique<LocalVector<StringName>>();
            ID[i]->reserve(64);
        }
    }

    ~MainThreadManager() {
        for (int i = 0; i < 2; ++i) {
            BUFFER[i]->clear();
            ID[i]->clear();
        }
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
    Mutex mutex;
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
            else {
                MutexLock mutex_lock(mutex);
                CURRENT_TASK = queue.front();			// tasks do exist, copy the front of the queue
            }

            // all arguements are bound BEFORE being added to task queue -> no arguements needed here
            PROCESSING.store(true, std::memory_order_acquire);
            CURRENT_TASK.call();
            PROCESSING.store(false, std::memory_order_acquire);
                
            // only safe to remove AFTER call(), due to main thread checking task queue
            MutexLock mutex_lock(mutex);
            queue.pop_front();
            SIZE--;
            if (queue.empty()) {    // .empty() check could be moved directly into .store(), but would require atomics to activate when not needed
                EMPTY.store(true, std::memory_order_acquire);
            }
        }
    }
};

struct TaskThreadManager {
    std::unique_ptr<ThreadTaskQueue[]> TASKS;
    real_t MAX_QUEUE_SIZE = 64;
    int thread_count = 0;

    /*
    - If a task queue is not processing, add to that queue
    - If all task queues are processing, add the task to the smallest queue
        - size determined by number of tasks (should probably add weights)
    */
    void insert_task(StringName task_name, Callable task_callable) {
        int min = 0;
        for (int i: range_flip(1, thread_count)) {
            if (!TASKS[i].PROCESSING.load()) {
                min = i;
                break;
            }
            else if (TASKS[i].PROCESSING.load() && TASKS[i].SIZE < TASKS[min].SIZE) {
                min = i;
                continue;
            }
        }
        //print_line("INSERT INTO THREAD: ", min);
        //print_line("NUMBER OF THREADS: ", thread_count);
        {
            MutexLock mutex_lock(TASKS[min].mutex);
            TASKS[min].queue.insert(task_name, task_callable);
            TASKS[min].SIZE++;
        }

        // task queue is no longer empty -> 'wake up' task thread (thread not actually sleeping)
        TASKS[min].EMPTY.store(false, std::memory_order_acquire);
    }

    bool has_task(StringName task_name) {
        for (int i = 0; i < thread_count; ++i) {
            MutexLock mutex_lock(TASKS[i].mutex);
            if (TASKS[i].queue.has(task_name))
                return true;
        }
        return false;
    }

    TaskThreadManager() {
        //thread_count = OS::get_singleton()->get_default_thread_pool_size() - 1; // -1 for safety
        thread_count = 4;
        TASKS = std::make_unique<ThreadTaskQueue[]>(thread_count);

        for (int i = 0; i < thread_count; ++i) {
            TASKS[i].queue.set_capacity(MAX_QUEUE_SIZE);
            TASKS[i].thread.instantiate();
            TASKS[i].thread->start(callable_mp(&TASKS[i], &ThreadTaskQueue::thread_process), CoreBind::Thread::PRIORITY_NORMAL);
        }
    }

    ~TaskThreadManager() {
        for (int i = 0; i < thread_count; ++i) {
            TASKS[i].RUNNING.store(false, std::memory_order_acquire);
            TASKS[i].thread->wait_to_finish();
            TASKS[i].thread.unref();
        }
    }
};