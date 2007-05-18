#include "mailstore.hpp"

MailStore::MailStore(ImapSession *session) : session(session)
{}

MailStore::~MailStore()
{}
