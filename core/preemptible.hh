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
 * Copyright (C) 2016 ScyllaDB.
 */

#pragma once

#include "timer.hh"
#include <boost/intrusive/list.hpp>
#include <experimental/optional>

namespace seastar {

struct preemptible final {
    timer<> _sched_timer{[this] { restart_preempted_when_timer_fires(); }};
    std::experimental::optional<promise<>> _sched_promise;

public:
    template<typename TimeOrDuration>
    future<> go_dormant(TimeOrDuration);

    preemptible() = default;
    preemptible(const preemptible&) = delete;
    preemptible(preemptible&&) = delete;
    preemptible& operator=(preemptible&&) = delete;

private:
    boost::intrusive::list_member_hook<> _link;

    using preempted_item_list = boost::intrusive::list<preemptible,
        boost::intrusive::member_hook
            <preemptible,
             boost::intrusive::list_member_hook<>,
             &preemptible::_link>,
        boost::intrusive::constant_time_size<false>>;

    static thread_local preempted_item_list _preempted_items;

    void restart_preempted_when_timer_fires();
    void restart_preempted_when_cpu_idle();

    friend class ::reactor;
    // To be used by seastar reactor only.
    static bool try_run_one_yielded_item();
};

template<typename TimeOrDuration>
inline
future<> preemptible::go_dormant(TimeOrDuration when) {
    _preempted_items.push_back(*this);
    _sched_promise.emplace();
    _sched_timer.arm(when);
    return _sched_promise->get_future();
}

inline
void preemptible::restart_preempted_when_timer_fires() {
    _preempted_items.erase(_preempted_items.iterator_to(*this));
    _sched_promise->set_value();
    _sched_promise = {};
}

inline
void preemptible::restart_preempted_when_cpu_idle() {
    _sched_timer.cancel();
    _sched_promise->set_value();
    _sched_promise = {};
}

inline
bool preemptible::try_run_one_yielded_item() {
    if (seastar::preemptible::_preempted_items.empty()) {
        return false;
    }
    auto&& p = seastar::preemptible::_preempted_items.front();
    p.restart_preempted_when_cpu_idle();
    seastar::preemptible::_preempted_items.pop_front();
    return true;
}

}
