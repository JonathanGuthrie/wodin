/*
 * Copyright 2013 Jonathan R. Guthrie
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "locktestmaster.hpp"

LockTestMaster::LockTestMaster(std::string fqdn,
			       unsigned login_timeout, 
			       unsigned idle_timeout,
			       unsigned asynchronous_event_time,
			       unsigned bad_login_pause,
			       unsigned max_retries,
			       unsigned retry_seconds) : ImapMaster(fqdn, login_timeout, idle_timeout, asynchronous_event_time, bad_login_pause, max_retries, retry_seconds) {
    m_commands = NULL;
    m_responses = NULL;
}

LockTestMaster::~LockTestMaster() {}

void LockTestMaster::reset(void) {
    delete m_commands;
    delete m_responses;
    m_commands = NULL;
    m_responses = NULL;
}

void LockTestMaster::commands(const Svector &set) {
    m_commands = new Svector(set);
}

const LockTestMaster::Svector &LockTestMaster::commands(void) const {
    return *m_commands;
}

const LockTestMaster::Svector &LockTestMaster::responses(void) const {
    return *m_responses;
}
