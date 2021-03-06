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

#if !defined(_IMAPMASTER_HPP_INCLUDED_)
#define _IMAPMASTER_HPP_INCLUDED_

#include <clotho/servermaster.hpp>

#include "imapsession.hpp"

class SessionDriver;
class ImapUser;
class Namespace;

class ImapMaster : public ServerMaster {
public:
    ImapMaster(std::string fqdn, unsigned login_timeout = 60,
	       unsigned idle_timeout = 1800, unsigned asynchronous_event_time = 900, unsigned bad_login_pause = 5, unsigned max_retries = 12, unsigned retry_seconds = 5);
    virtual ~ImapMaster(void);
    virtual ImapSession *newSession(SessionDriver *driver, Server *server);

    ImapUser *userInfo(const char *userid);
    Namespace *mailStore(ImapSession *session);
    bool isAnonymousEnabled() { return false; }
    bool isLoginEnabledByPolicy(void) const;
    // This will send the specified message out the specified socket
    // after the specified number of seconds and then set that session
    // up to receive again
    time_t idleTimeout(void) const { return m_idleTimeout; /* seconds */ }
    time_t loginTimeout(void) const { return m_loginTimeout; /* seconds */ }
    time_t asynchronousEventTime(void) const { return m_asynchronousEventTime; /* seconds */ }
    const std::string &fqdn(void) const { return m_fqdn; }
    bool useConfiguredUid(void) const { return m_useConfiguredUid; }
    uid_t configuredUid(void) const { return m_configuredUid; }
    bool useConfiguredGid(void) const { return m_useConfiguredGid; }
    uid_t configuredGid(void) const { return m_configuredGid; }
    void fqdn(const std::string &fqdn) { m_fqdn = fqdn; }
    unsigned badLoginPause(void) const { return m_badLoginPause; }
    unsigned maxRetries(void) const { return m_maxRetries; }
    unsigned retryDelaySeconds(void) const { return m_retryDelaySeconds; }

    const std::string &keyfile(void) const { return m_keyfile; }
    const std::string &certfile(void) const { return m_certfile; }
    const std::string &cafile(void) const { return m_cafile; }
    const std::string &crlfile(void) const { return m_crlfile; }

#if 0
    void idleTimer(SessionDriver *driver, unsigned seconds);
    void scheduleAsynchronousAction(SessionDriver *driver, unsigned seconds);
    void scheduleRetry(SessionDriver *driver, unsigned seconds);
    void delaySend(SessionDriver *driver, unsigned seconds, const std::string &message);
#endif // 0

private:
    unsigned m_idleTimeout;
    unsigned m_loginTimeout;
    unsigned m_asynchronousEventTime;
    bool m_useConfiguredUid;
    uid_t m_configuredUid;
    bool m_useConfiguredGid;
    gid_t m_configuredGid;
    std::string m_fqdn;
    unsigned m_badLoginPause;
    unsigned m_maxRetries;
    unsigned m_retryDelaySeconds;
    std::string m_keyfile;
    std::string m_certfile;
    std::string m_cafile;
    std::string m_crlfile;
};

#endif //_IMAPMASTER_HPP_INCLUDED_
