#pragma once

namespace KERF_NAMESPACE {

void cleanup_threads();


#pragma mark - Thread

struct THREAD
{
  static void * thread_start(void *arg);
  static void*  thread_noop(void *arg);
  static void handle_SIGUSR2(int sig);
  static void handle_SIGABRT(int sig);

protected: // POP it's just easier for now if we make every thread init, register with a normalized id, etc. Otherwise we have to track a separate pool [separate parallel registration process for non-kerf_vm_stack thread ids and such] when we want to deliver signals. Also, before disabling this, consider using a thread pool.
  bool should_init_deinit_kerf_vm_thread_stuff = true; // POP false: saves resources, true: enables kerfvm use, pools, etc.
public:
  // Question. Should we change kerf thread to accept lambdas? Answer. Probably not because then you have to manage the lambdas lifetimes. We can use lambdas in the pool however.
  decltype(&thread_start) function_to_call = &thread_start;
  void *function_data = nullptr;

  pthread_t thr;
  pthread_attr_t attr;
  I normalized_id = -1;

  decltype(PTHREAD_CREATE_JOINABLE) detach_state = PTHREAD_CREATE_DETACHED;
  decltype(PTHREAD_SCOPE_PROCESS) scope = PTHREAD_SCOPE_SYSTEM;
  decltype(PTHREAD_EXPLICIT_SCHED) inherit_sched = PTHREAD_INHERIT_SCHED;
  decltype(SCHED_OTHER) sched_policy = SCHED_FIFO; // POSIX: SCHED_FIFO SCHED_RR SCHED_SPORADIC SCHED_OTHER
  struct sched_param param = {.sched_priority=(sched_get_priority_max(sched_policy) + sched_get_priority_min(sched_policy))/2};
  decltype(PTHREAD_CANCEL_DEFERRED) cancel_type = PTHREAD_CANCEL_ASYNCHRONOUS;
  decltype(PTHREAD_CANCEL_DISABLE) cancel_state = PTHREAD_CANCEL_ENABLE; 

  static void * kerf_init_wrapper(void *arg);

  static void handle_error_en(int en, const char* msg);

  int init();

  void deinit();

  THREAD() = default;
  ~THREAD() = default;
  THREAD(const THREAD &) = default; // we rely on default copy constructor currently

  int start();

  int join(void **retval = nullptr);

};

#pragma mark - THREAD_POOL

template<typename T = std::function<void()>>
struct THREAD_POOL : REGISTERED_FOR_LONGJMP_WORKSTACK
{
  // Observation. A thread pool with 1 thread [that accepts lambdas] is a synchronous queue, with 2+ threads asynchronous.

  // Remark. A nice improvement would be to lazily allocate threads. Then we could cap at 1024 say for an async queue but not actually acquire that many threads if we weren't using them. With a limit of 8 say and 8 long running jobs pinned the remaining 100 small jobs will starve instead of flushing. 

  std::deque<T> jobs;

  MUTEX_UNTRACKED mutex;
  pthread_cond_t work_avail; // Question. should we munge this onto MUTEX? Answer. Doesn't seem necessary at this point.
  pthread_cond_t inactivity;

  I active_job_count = 0;

  std::vector<THREAD> threads;

  std::atomic<bool> time_to_destruct = false;

  static void* thread_pool_work_loop(void *v)
  {
    int s;
    THREAD_POOL *pool = (THREAD_POOL*)v;

    bool predicate;

    while(!pool->time_to_destruct)
    {
      pool->mutex.lock();

      while(!(predicate = pool->jobs.size() > 0) && !pool->time_to_destruct)
      {
        s = pthread_cond_wait(&pool->work_avail, &pool->mutex.pmutex);
        if(s != 0)
        {
          THREAD::handle_error_en(s, "pthread_cond_wait");
        }
      }

      if(predicate && !pool->time_to_destruct)
      {
        auto j = std::move(pool->jobs.front());
        pool->jobs.pop_front();
        __sync_fetch_and_add(&pool->active_job_count, 1);
        pool->mutex.unlock();
        j();

        // this second lock is entirely to prevent a race condition for the `.wait()` functionality
        pool->mutex.lock_safe_wrapper([&]{ // this obviates __sync here
          __sync_fetch_and_sub(&pool->active_job_count, 1);
          s = pthread_cond_signal(&pool->inactivity);
        });

        if (s != 0)
        {
          THREAD::handle_error_en(s, "thread pool pthread_cond_signal error");
        }

      }
      else
      {
        pool->mutex.unlock();
      }
    }

    return nullptr;
  }

  void init_helper(I threadcount = -1)
  {
    int s;

    s = pthread_cond_init(&this->work_avail, nullptr);
    if(s!=0)
    {
      THREAD::handle_error_en(s, "thread pool pthread_cond_init 1 error");
    }

    s = pthread_cond_init(&this->inactivity, nullptr);
    if(s!=0)
    {
      THREAD::handle_error_en(s, "thread pool pthread_cond_init 2 error");
    }

    if (-1 == threadcount)
    {
      threadcount = kerf_count_hardware_threads();
    }

    DO(threadcount,

      threads.emplace_back(THREAD());
      threads.size();

      threads[i].detach_state = PTHREAD_CREATE_JOINABLE;
      threads[i].function_to_call = thread_pool_work_loop;
      threads[i].function_data = this;

      int r = threads[i].start();

      if(r != 0)
      {
        THREAD::handle_error_en(r, "thread pool start thread");
      }
    )
  }


  THREAD_POOL(I threadcount = -1)
  {
    init_helper(threadcount);
  }

  THREAD_POOL(THREAD_POOL &) = delete;

  ~THREAD_POOL()
  {
    mutex.lock_safe_wrapper([&]{
      // NB. this lock was necessary to solve a subtle bug: worker threads can
      // pass the `while` check with 0==time_to_destruct and then after it goes
      // to 1, we'd broadcast here, then after that they'd enter work_avail
      // condition and wait forever
      time_to_destruct = true;
    });

    pthread_cond_broadcast(&this->work_avail);

    // 2021.10.19 Remark. putting
    // wait();
    // here solves certain ctrl-c bugs (eg, you put a thread pool in main with while(1) looping threads, but don't pool.wait() before exit(), then ctrl-c the abandoned threads), but i haven't played with it much. That the child thread would crash if the POOL was freed before its child thread seems obvious in retrospect. We probably need to enable this wait() on destruct. What's weird though is that it joins() below, so that should be sufficient to "wait". So what's the difference? also...what "extra" does joining here accomplish versus just waiting? looking at "->wait()", it might not survive ctrl-c because the counts will cause it to sleep indefinitely (but then it will eventually be killed by ctrl-c). I don't understand this. wrapping the joins in a critical section didn't work. getting a signal in a mutex may let you go back to sleep?
    // Note: we *may possibly* WANT the threads to crash if you don't call wait() yourself, depending on our desired model/regime. it might make more sense to require a paired wait() in c++ land to avoid crashes.
    // 2021.10.23 Observation, it's possible this was screwed up only temporarily while we had the undiscovered bug in popping the wrapper guards where they weren't freed. but i haven't gone back to look.

    for(auto &t : threads)
    {
      auto result = t.join();
    }

    pthread_cond_destroy(&this->work_avail);
    pthread_cond_destroy(&this->inactivity);
  }

  void wait()
  {
    int s;

    mutex.lock_safe_wrapper([&]{
      while(jobs.size() > 0 || active_job_count > 0)
      {
        s = pthread_cond_wait(&this->inactivity, &this->mutex.pmutex);
        if(s != 0)
        {
          THREAD::handle_error_en(s, "pthread_cond_wait 2");
        }
      }
    });
  }

  // Idea.
  // this->pause(): signal the threads to `int ::pause(void);` (from #include <unistd.h>) via pthread_kill
  // (have a suite of SIGUSR2 actions: thread_cleanup, pause, etc., using a per-thread action variable enum)
  // this->resume(): then signal all threads again with a noop action to escape `pause()`
  // (in the destructor, call resume() after time_to_destruct=true but before pthread_cond_broadcast)
  // void pause(){}
  // void resume(){}


#if !THREAD_POOL_SUPPORTS_FUTURES

  template<typename L> 
  void add(L job)
  {
    mutex.lock_safe_wrapper([&]{
      jobs.emplace_back(job);
    });
  
    int s;
    s = pthread_cond_signal(&this->work_avail);
    if (s != 0) 
    {
      THREAD::handle_error_en(s, "thread pool pthread_cond_signal error");
    }
  }

#else

  // This enables:
  // auto result = pool.add([]{return 22;});
  // auto rg = result.get();

  template<typename F, class... Args> 
  auto add(F f, Args&&... args) -> std::future<typename std::result_of<F(Args...)>::type>
  {
    using return_type = typename std::result_of<F(Args...)>::type;

    auto job = std::make_shared< std::packaged_task<return_type()> >(
      std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );
        
    std::future<return_type> res = job->get_future();

    mutex.lock_safe_wrapper([&]{
      jobs.emplace_back([job](){(*job)();});
    });

    int s;
    s = pthread_cond_signal(&this->work_avail);
    if (s != 0) 
    {
      THREAD::handle_error_en(s, "thread pool pthread_cond_signal error");
    }

    return res;
  }

#endif
};


} // namespace
