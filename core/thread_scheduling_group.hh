/*
 * This file is open source software, licensed to you under the terms
 * of the Apache License, Version 2.0 (the "License").  See the NOTICE file
 * distributed with this work for additional information regarding copyright
 * ownership.  You may not use this file except in compliance with the License.
 *
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

/*
 * Copyright (C) 2016 Cloudius Systems, Ltd.
 */

#pragma once

// Forward declare for friendship
namespace seastar { class thread_scheduling_group; }
template<typename AsyncAction>
static future<> preemptible_repeat(seastar::thread_scheduling_group*, AsyncAction&&);

/// Seastar API namespace
namespace seastar {

namespace stdx = std::experimental;
namespace bi = boost::intrusive;

// TODO(peterj): This class in now used not only for threads but also for futures. So might be
// appropriate to remove the 'thread_' prefix from the name.

/// An instance of this class can be used to assign a thread to a particular scheduling group.
/// Threads can share the same scheduling group if they hold a pointer to the same instance
/// of this class.
///
/// All threads that belongs to a scheduling group will have a time granularity defined by \c period,
/// and can specify a fraction \c usage of that period that indicates the maximum amount of time they
/// expect to run. \c usage, is expected to be a number between 0 and 1 for this to have any effect.
/// Numbers greater than 1 are allowed for simplicity, but they just have the same meaning of 1, alas,
/// "the whole period".
///
/// Note that this is not a preemptive runtime, and a thread will not exit the CPU unless it is scheduled out.
/// In that case, \c usage will not be enforced and the thread will simply run until it loses the CPU.
/// This can happen when a thread waits on a future that is not ready, or when it voluntarily call yield.
///
/// Unlike what happens for a thread that is not part of a scheduling group - which puts itself at the back
/// of the runqueue everytime it yields, a thread that is part of a scheduling group will only yield if
/// it has exhausted its \c usage at the call to yield. Therefore, threads in a schedule group can and
/// should yield often.
///
/// After those events, if the thread has already run for more than its fraction, it will be scheduled to
/// run again only after \c period completes, unless there are no other tasks to run (the system is
/// idle)
class thread_scheduling_group {
    std::chrono::nanoseconds _period;
    std::chrono::nanoseconds _quota;
    std::chrono::time_point<thread_clock> _this_period_ends = {};
    std::chrono::time_point<thread_clock> _this_run_start = {};
    std::chrono::nanoseconds _this_period_remain = {};
public:
    /// \brief Constructs a \c thread_scheduling_group object
    ///
    /// \param period a duration representing the period
    /// \param usage which fraction of the \c period to assign for the scheduling group. Expected between 0 and 1.
    thread_scheduling_group(std::chrono::nanoseconds period, float usage);
    /// \brief changes the current maximum usage per period
    ///
    /// \param new_usage The new fraction of the \c period (Expected between 0 and 1) during which to run
    void update_usage(float new_usage) {
        _quota = std::chrono::duration_cast<std::chrono::nanoseconds>(new_usage * _period);
    }
private:
    stdx::optional<thread_clock::time_point> next_scheduling_point() const;
    void account_start();
    void account_stop();
    friend class thread_context;
    template<typename AsyncAction>
    friend future<> (::preemptible_repeat)(thread_scheduling_group*, AsyncAction&&);
};

}
