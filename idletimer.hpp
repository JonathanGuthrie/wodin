#if !defined(_DELTAQUEUEIDLETIMER_HPP_INCLUDED_)
#define _DELTAQUEUEIDLETIMER_HPP_INCLUDED_

#include <clotho/deltaqueueaction.hpp>
#include <clotho/internetsession.hpp>

// The idle timer is never reset.  Instead, I keep the time the last command
// was executed and then check for the timeout period elapsing when the timer expires
// and set the timeout for the time since that command happened.
class DeltaQueueIdleTimer : public DeltaQueueAction
{
public:
    DeltaQueueIdleTimer(int delta, InternetSession *session);
    virtual void HandleTimeout(bool isPurge);
};


#endif // _DELTAQUEUEIDLETIMER_HPP_INCLUDED_
