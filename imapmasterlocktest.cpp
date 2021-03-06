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

#include <clotho/internetserver.hpp>

#include "imaplocktestuser.hpp"
#include "mailstorelocktest.hpp"
#include "namespace.hpp"
#include "imapmaster.hpp"

ImapMaster::ImapMaster(std::string fqdn, unsigned login_timeout, unsigned idle_timeout, unsigned asynchronous_event_time, unsigned bad_login_pause, unsigned max_retries, unsigned retry_seconds) {
    m_useConfiguredUid = false;
    m_configuredUid = 0;
    m_useConfiguredGid = false;
    m_configuredGid = 0;
    m_loginTimeout = login_timeout;
    m_idleTimeout = idle_timeout;
    m_asynchronousEventTime = asynchronous_event_time;
    m_badLoginPause = bad_login_pause;
    m_maxRetries = max_retries;
    m_retryDelaySeconds = retry_seconds;

    ImapSession::buildSymbolTables();
}

ImapMaster::~ImapMaster(void) {
}

ImapUser *ImapMaster::userInfo(const char *userid) {
    return (ImapUser *) new ImapLockTestUser(userid, this);
}

Namespace *ImapMaster::mailStore(ImapSession *session) {
    Namespace *result = new Namespace(session);
    result->addNamespace(Namespace::PERSONAL, "", (MailStore *) new MailStoreLockTest(session), '/');

    return result;
}


ImapSession *ImapMaster::newSession(SessionDriver *driver, Server *server) {
    return new ImapSession(this, driver, server);
}

