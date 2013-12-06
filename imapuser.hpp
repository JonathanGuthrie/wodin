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

#if !defined(_IMAPUSER_HPP_INCLUDED_)
#define _IMAPUSER_HPP_INCLUDED_

#include <string>
#include <unistd.h>

class ImapMaster;

// SYZYGY -- I need to know whether HavePlaintextPassword will return true or not
// SYZYGY -- even under circumstances when I can't create a user because I haven't
// SYZYGY -- yet gotten that far.  For example, it alters the capability string if
// SYZYGY -- I have access to the plain text password because I can then do CRAM and
// SYZYGY -- other SASL methods.
// SYZYGY -- The two approaches I've come up with so far are these:  Put a HavePlaintextPassword
// SYZYGY -- method into the server and (possibly) make that method static in the descendents
// SYZYGY -- of ImapUser and create a user when the session is created, and then have a 
// SYZYGY -- SetUsername method that will do the stuff that the constructor does now.
// SYZYGY -- I don't know which I like better.
class ImapUser {
public:
    ImapUser(const char *user, const ImapMaster *server);
    virtual ~ImapUser();
    virtual bool havePlaintextPassword() const = 0;
    virtual bool checkCredentials(const char *password) const = 0;
    virtual char *password(void) const = 0;
    virtual char *homeDir(void) const = 0;
    const char *name(void) const { return m_name->c_str(); }
    virtual std::string *expandPath(const std::string &specifier) const;
    uid_t uid(void) const { return m_uid; }
    gid_t gid(void) const { return m_gid; }

protected:
    std::string *m_name;
    uid_t m_uid;
    gid_t m_gid;
    bool m_userFound;
};

#endif // _IMAPUSER_HPP_INCLUDED_
