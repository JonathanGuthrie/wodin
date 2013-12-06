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

#if !defined(_IMAPUNIXUSER_HPP_INCLUDED_)
#define _IMAPUNIXUSER_HPP_INCLUDED_

#include <string>

#include "imapuser.hpp"

class ImapUnixUser : ImapUser {
public:
    ImapUnixUser(const char *user, const ImapMaster *Master);
    virtual ~ImapUnixUser();
    virtual bool havePlaintextPassword() const;
    virtual bool checkCredentials(const char *password) const;
    virtual char *password(void) const;
    virtual char *homeDir(void) const { return m_home; };

private:
    char *m_home;
};

#endif // _IMAPUNIXUSER_HPP_INCLUDED_
