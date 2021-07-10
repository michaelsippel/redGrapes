
#pragma once

#include <thread>

#include <redGrapes/graph/scheduling_graph.hpp>

#include <redGrapes/scheduler/scheduler.hpp>
#include <redGrapes/scheduler/fifo.hpp>
#include <redGrapes/scheduler/worker.hpp>

namespace redGrapes
{
namespace scheduler
{

/*
 * Combines a FIFO with worker threads
 */
template <
    typename TaskID,
    typename TaskPtr
>
struct DefaultScheduler : public IScheduler< TaskID, TaskPtr >
{
    using EventID = typename redGrapes::SchedulingGraph< TaskID, TaskPtr >::EventID;

    std::mutex m;
    std::condition_variable cv;
    std::atomic_flag wait = ATOMIC_FLAG_INIT;

    std::shared_ptr< redGrapes::scheduler::FIFO< TaskID, TaskPtr > > fifo;
    std::vector< std::shared_ptr< redGrapes::scheduler::WorkerThread<> > > threads;
    redGrapes::scheduler::DefaultWorker main_thread_worker;

    DefaultScheduler( size_t n_threads ) ://std::thread::hardware_concurrency() ) :
        main_thread_worker( [this]{ return false; } ),
        fifo( std::make_shared< redGrapes::scheduler::FIFO< TaskID, TaskPtr > >() )
    {
        for( size_t i = 0; i < n_threads; ++i )
            threads.emplace_back(
                 std::make_shared< redGrapes::scheduler::WorkerThread<> >(
                     [this] { return this->fifo->consume(); }
                 )
            );

        thread::idle =
            [this]
            {
                std::unique_lock< std::mutex > l( m );
                cv.wait( l, [this]{ return !wait.test_and_set(); } );
            };
    }

    void init_mgr_callbacks(
        std::shared_ptr< redGrapes::SchedulingGraph< TaskID, TaskPtr > > scheduling_graph,
        std::function< bool () > advance,
        std::function< bool ( TaskPtr ) > run_task,
        std::function< bool ( TaskPtr ) > activate,
        std::function< void ( TaskPtr ) > activate_followers,
        std::function< void ( TaskPtr ) > remove_task
    )
    {
        fifo->init_mgr_callbacks( scheduling_graph, advance, run_task, activate, activate_followers, remove_task );
    }

    //! wakeup sleeping worker threads
    void notify()
    {
        spdlog::trace("scheduler notify");
        {
            std::unique_lock< std::mutex > l( m );
            wait.clear();
        }
        cv.notify_one();

        // todo : wakeup one worker at max
        for( auto & thread : threads )
            thread->worker.notify();
    }

    bool
    activate_task( TaskPtr task_ptr )
    {
        return fifo->activate_task( task_ptr );
    }
};

/*! Factory function to easily create a default-scheduler object
 */
template <
    typename Manager
>
auto make_default_scheduler(
    Manager & m,
    size_t n_threads = std::thread::hardware_concurrency()
)
{
    return std::make_shared<
               DefaultScheduler<
                   typename Manager::TaskID,
                   typename Manager::TaskPtr
               >
           >( n_threads );
}

} // namespace scheduler

} // namespace redGrapes

