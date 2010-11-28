#if !defined(_DELTAQUEUERETRY_HPP_INCLUDED_)
#define _DELTAQUEUERETRY_HPP_INCLUDED_

#include <clotho/deltaqueueaction.hpp>

#include "mailstore.hpp"

class SessionDriver;

// When the timer expires, execute the check for mailbox changes method in the selected
// mail store.
class DeltaQueueRetry : public DeltaQueueAction
{
public:
    DeltaQueueRetry(int delta, InternetSession *session);
    virtual void HandleTimeout(bool isPurge);

private:
};


#endif // _DELTAQUEUEASYNCHRONOUSACTION_HPP_INCLUDED_
