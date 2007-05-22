#if !defined(_DELTAQUEUEIDLETIMER_HPP_INCLUDED_)
#define _DELTAQUEUEIDLETIMER_HPP_INCLUDED_

#include "deltaqueueaction.hpp"

class SessionDriver;

// The idle timer is never reset.  Instead, I keep the time the last command
// was executed and then check for the timeout period elapsing when the timer expires
// and set the timeout for the time since that command happened.
class DeltaQueueIdleTimer : DeltaQueueAction
{
public:
    DeltaQueueIdleTimer(int delta, SessionDriver *driver);
    virtual void HandleTimeout(bool isPurge);
};


#endif // _DELTAQUEUEIDLETIMER_HPP_INCLUDED_
