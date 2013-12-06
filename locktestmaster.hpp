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

#if !defined(_LOCKTESTMASTER_HPP_INCLUDED_)
#define _LOCKTESTMASTER_HPP_INCLUDED_

#include "imapmaster.hpp"

#include <vector>

class LockTestMaster : public ImapMaster {
public:
    typedef std::vector<std::string> Svector;

    LockTestMaster(std::string fqdn, unsigned login_timeout = 60,
		   unsigned idle_timeout = 1800, unsigned asynchronous_event_time = 900, unsigned bad_login_pause = 5, unsigned max_retries = 12, unsigned retry_seconds = 5);
    virtual ~LockTestMaster(void);
    void reset(void);
    const Svector &commands(void) const;
    void commands(const Svector &set);
    const Svector &responses(void) const;
    void response(const std::string &response);

private:
    Svector *m_commands;
    Svector *m_responses;
};

#endif //_LOCKTESTMASTER_HPP_INCLUDED_
