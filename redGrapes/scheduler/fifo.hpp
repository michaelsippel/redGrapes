/* Copyright 2019-2020 Michael Sippel
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <mutex>
#include <queue>
#include <optional>

#include <redGrapes/scheduler/scheduler.hpp>
#include <redGrapes/thread_local.hpp>

namespace redGrapes
{
namespace scheduler
{

template <
    typename TaskID,
    typename TaskPtr
>
struct FIFO : public SchedulerBase< TaskID, TaskPtr >
{
private:

    // active = ready, running, paused, done (but not finished)
    std::mutex mutex_active_tasks;
    std::unordered_set< TaskID > active_tasks;

    std::mutex mutex_running_tasks;
    std::list< TaskPtr > running_tasks; // running or paused

    std::mutex mutex_ready_tasks;
    std::queue< TaskPtr > ready_tasks;

    std::mutex mutex_done_tasks;
    std::vector< std::pair< TaskID, TaskPtr > > done_tasks;

public:
    //! returns true if a job was consumed, false if queue is empty
    bool consume()
    {
        if( auto task_ptr = get_job() )
        {
            auto task_id = task_ptr->locked_get().task_id;
            this->scheduling_graph->task_start( task_id );

            {
                std::unique_lock< std::mutex > l( mutex_running_tasks );
                running_tasks.push_back( *task_ptr );
            }

            bool finished = this->mgr_run_task( *task_ptr );

            if( finished )
            {
                spdlog::trace("task {} finished", task_id);
                this->scheduling_graph->task_end( task_id );
            }

            {
                std::unique_lock< std::mutex > l_active( mutex_active_tasks );
                active_tasks.erase( task_id );
            }

            if( finished )
            {
                {
                    std::unique_lock< std::mutex > l_d( mutex_done_tasks );
                    std::unique_lock< std::mutex > l_r( mutex_running_tasks );

                    done_tasks.push_back( std::make_pair(task_id, *task_ptr) );
                    running_tasks.remove( *task_ptr );
                }
            }

            return true;
        }
        else
            return false;
    }

    //! checks for a pending or paused task if it is ready
    //! precedence graph must be locked
    bool activate_task( TaskPtr task_ptr )
    {
        std::unique_lock< std::mutex > l_active( mutex_active_tasks );

        auto task_id = task_ptr.get().task_id;
        spdlog::trace("activate task {}", task_id);

        if( ! active_tasks.count(task_id) )
        {
            if( ! this->scheduling_graph->exists_task( task_id ) )
                this->scheduling_graph->add_task( task_ptr );

            if( ! this->scheduling_graph->is_task_finished( task_id ) )
            {
                if( this->scheduling_graph->is_task_ready( task_id ) )
                {
                    active_tasks.insert( task_id );

                    std::unique_lock< std::mutex > l_ready( mutex_ready_tasks );
                    ready_tasks.push( task_ptr );

                    return true;
                }
            }
        }

        return false;
    }

private:
    //! get a task from the ready queue if available
    std::optional< TaskPtr > get_job()
    {
        std::unique_lock< std::mutex > l_ready( mutex_ready_tasks );

        if( ready_tasks.empty() )
        {
            // try to advance the calculation
            // until we have something in the ready queue
            l_ready.unlock();
            update();
            l_ready.lock();

            // if the queue is still empty, the worker has to wait
            if( ready_tasks.empty() )
                return std::nullopt;
        }

        // we have a ready task
        auto task_ptr = std::move( ready_tasks.front() );
        ready_tasks.pop();

        return task_ptr;
    }

    //! try to get at least one task into the ready queue
    void update()
    {
        {
            // remove done tasks

            std::unique_lock< std::mutex > l_done( mutex_done_tasks );
            for( size_t i = 0; i < done_tasks.size(); ++i )
            {
                auto task_id = done_tasks[i].first;
                auto task_ptr = done_tasks[i].second;

                if( auto graph = task_ptr.get_subgraph() )
                {
                    auto grlock = graph->unique_lock();
                    while( auto new_task_vertex = graph->advance() )
                    {
                        grlock.unlock();
                        if( this->mgr_activate(TaskPtr{ graph, *new_task_vertex }) )
                        {
                            /*
                            std::unique_lock< std::mutex > l_ready( mutex_ready_tasks );
                            if( ! ready_tasks.empty() )
                                return;
                            */
                        }
                        grlock.lock();
                    }
                }

                if( this->scheduling_graph->is_task_finished( task_id ) )
                {
                    spdlog::trace("erase done task {}", task_id);
                    done_tasks.erase( done_tasks.begin() + i );

                    -- i;

                    this->mgr_activate_followers( task_ptr ); // will take mutex_active_tasks and mutex_ready_tasks
                    this->mgr_remove_task( task_ptr );
                }
            }
        }
        /*
        {
            std::unique_lock< std::mutex > l_ready( mutex_ready_tasks );
            if( ! ready_tasks.empty() )
                return;
        }
        */
        // advance subgraphs until a ready task appears
        {
            std::unique_lock< std::mutex > l_running( mutex_running_tasks );
            for( auto task_ptr : running_tasks )
            {
                //spdlog::info("check running task {}", task_ptr.locked_get().task_id);
                auto graph = task_ptr.get_subgraph();
                if( graph )
                {
                    auto grlock = graph->unique_lock();
                    while( auto new_task_vertex = graph->advance() )
                    {
                        grlock.unlock();
                        TaskPtr new_task_ptr{ graph, *new_task_vertex };
                        if( this->mgr_activate( new_task_ptr ) )
                        {
                            /*
                            std::unique_lock< std::mutex > l_ready( mutex_ready_tasks );
                            if( ! ready_tasks.empty() )
                                return;
                            */
                        }
                        grlock.lock();
                    }
                }
            }
        }

        if( this->mgr_advance )
            while( this->mgr_advance() )
            {
                std::unique_lock< std::mutex > l_ready( mutex_ready_tasks );
                if( ! ready_tasks.empty() )
                    return;
            }
    }
};

/*! Factory function to easily create a fifo-scheduler object
 */
template <
    typename Manager
>
auto make_fifo_scheduler(
    Manager & m
)
{
    return std::make_shared<
               FIFO<
                   typename Manager::TaskID,
                   typename Manager::TaskPtr
               >
           >();
}

} // namespace scheduler

} // namespace redGrapes

