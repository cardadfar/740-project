/*
    Copyright 2005-2012 Intel Corporation.  All Rights Reserved.

    This file is part of Threading Building Blocks.

    Threading Building Blocks is free software; you can redistribute it
    and/or modify it under the terms of the GNU General Public License
    version 2 as published by the Free Software Foundation.

    Threading Building Blocks is distributed in the hope that it will be
    useful, but WITHOUT ANY WARRANTY; without even the implied warranty
    of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Threading Building Blocks; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    As a special exception, you may use this file as part of a free software
    library without restriction.  Specifically, if other files instantiate
    templates or use macros or inline functions from this file, or you compile
    this file and link it with other files to produce an executable, this
    file does not by itself cause the resulting executable to be covered by
    the GNU General Public License.  This exception does not however
    invalidate any other reasons why the executable file might be covered by
    the GNU General Public License.
*/

#ifndef __TBB_task_arena_H
#define __TBB_task_arena_H

#include "task.h"
#include "tbb_exception.h"

#if __TBB_TASK_ARENA

namespace tbb {

//! @cond INTERNAL
namespace internal {
    //! Internal to library. Should not be used by clients.
    /** @ingroup task_scheduling */
    class arena;
    class task_scheduler_observer_v3;
} // namespace internal
//! @endcond

namespace interface6 {
//! @cond INTERNAL
namespace internal {
using namespace tbb::internal;

template<typename F>
class enqueued_function_task : public task { // TODO: reuse from task_group?
    F my_func;
    /*override*/ task* execute() {
        my_func();
        return NULL;
    }
public:
    enqueued_function_task ( const F& f ) : my_func(f) {}
};

class delegate_base : no_assign {
public:
    virtual void run() = 0;
    virtual ~delegate_base() {}
};

template<typename F>
class delegated_function : public delegate_base {
    F &my_func;
    /*override*/ void run() {
        my_func();
    }
public:
    delegated_function ( F& f ) : my_func(f) {}
};
} // namespace internal
//! @endcond

/** 1-to-1 proxy representation class of scheduler's arena
 * Constructors set up settings only, real construction is deferred till the first method invocation
 * TODO: A side effect of this is that it's impossible to create a const task_arena object. Rethink?
 * Destructor only removes one of the references to the inner arena representation.
 * Final destruction happens when all the references (and the work) are gone.
 */
class task_arena {
    friend class internal::task_scheduler_observer_v3;
    //! Concurrency level for deferred initialization
    int my_max_concurrency;

    //! NULL if not currently initialized.
    internal::arena* my_arena;

    // const methods help to optimize the !my_arena check TODO: check, IDEA: move to base-class?
    internal::arena* __TBB_EXPORTED_METHOD internal_initialize( int ) const;
    void __TBB_EXPORTED_METHOD internal_terminate( );
    void __TBB_EXPORTED_METHOD internal_enqueue( task&, intptr_t ) const;
    void __TBB_EXPORTED_METHOD internal_execute( internal::delegate_base& ) const;
    void __TBB_EXPORTED_METHOD internal_wait() const;

    inline void check_init() {
        if( !my_arena )
            my_arena = internal_initialize( my_max_concurrency );
    }

public:
    //! Typedef for number of threads that is automatic.
    static const int automatic = -1; // any value < 1 means 'automatic'

    //! Creates task_arena with certain concurrency limit
    task_arena(int max_concurrency = automatic)
        : my_max_concurrency(max_concurrency)
        , my_arena(0)
    {}

    //! Copies settings from another task_arena
    task_arena(const task_arena &s)
        : my_max_concurrency(s.my_max_concurrency) // copy settings
        , my_arena(0) // but not the reference or instance
    {}

    //! Removes the reference to the internal arena representation, and destroys the external object
    //! Not thread safe wrt concurrent invocations of other methods
    ~task_arena() {
        internal_terminate();
    }

    //! Enqueues a task into the arena to process a functor, and immediately returns.
    //! Does not require the calling thread to join the arena
    template<typename F>
    void enqueue( const F& f ) {
        check_init();
        internal_enqueue( *new( task::allocate_root() ) internal::enqueued_function_task<F>(f), 0 );
    }

#if __TBB_TASK_PRIORITY
    //! Enqueues a task with priority p into the arena to process a functor f, and immediately returns.
    //! Does not require the calling thread to join the arena
    template<typename F>
    void enqueue( const F& f, priority_t p ) {
        __TBB_ASSERT( p == priority_low || p == priority_normal || p == priority_high, "Invalid priority level value" );
        check_init();
        internal_enqueue( *new( task::allocate_root() ) internal::enqueued_function_task<F>(f), (intptr_t)p );
    }
#endif// __TBB_TASK_PRIORITY

    //! Joins the arena and executes a functor, then returns
    //! If not possible to join, wraps the functor into a task, enqueues it and waits for task completion
    //! Can decrement the arena demand for workers, causing a worker to leave and free a slot to the calling thread
    template<typename F>
    void execute(F& f) {
        check_init();
        internal::delegated_function<F> d(f);
        internal_execute( d );
    }

    //! Joins the arena and executes a functor, then returns
    //! If not possible to join, wraps the functor into a task, enqueues it and waits for task completion
    //! Can decrement the arena demand for workers, causing a worker to leave and free a slot to the calling thread
    template<typename F>
    void execute(const F& f) {
        check_init();
        internal::delegated_function<const F> d(f);
        internal_execute( d );
    }

    //! Wait for all work in the arena to be completed
    //! Even submitted by other application threads
    //! Joins arena if/when possible (in the same way as execute())
    void wait_until_empty() {
        check_init();
        internal_wait();
    }

    //! Sets concurrency level and initializes internal representation
    inline void initialize(int max_concurrency) {
        my_max_concurrency = max_concurrency;
        __TBB_ASSERT( !my_arena, "task_arena was initialized already"); // TODO: throw?
        check_init();
    }

    //! Returns the index, aka slot number, of the calling thread in its current arena
    static int __TBB_EXPORTED_FUNC current_slot();
};

} // namespace interfaceX

using interface6::task_arena;

} // namespace tbb

#endif /* __TBB_TASK_ARENA */

#endif /* __TBB_task_arena_H */
