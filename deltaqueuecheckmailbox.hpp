#if !defined(_DELTAQUEUECHECKMAILBOX_HPP_INCLUDED_)
#define _DELTAQUEUECHECKMAILBOX_HPP_INCLUDED_

#include "deltaqueueaction.hpp"
#include "mailstore.hpp"

class SessionDriver;

// When the timer expires, execute the check for mailbox changes method in the selected
// mail store.
class DeltaQueueCheckMailbox : DeltaQueueAction
{
public:
    DeltaQueueCheckMailbox(int delta, SessionDriver *driver, MailStore *store);
    virtual void HandleTimeout(bool isPurge);

private:
    MailStore *m_store;
};


#endif // _DELTAQUEUECHECKMAILBOX_HPP_INCLUDED_
