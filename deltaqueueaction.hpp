#if !defined(_DELTAQUEUEACTION_HPP_INCLUDED_)
#define _DELTAQUEUEACTION_HPP_INCLUDED_

#include "sessiondriver.hpp"

class DeltaQueueAction
{
public:
    DeltaQueueAction(int delta, SessionDriver *driver);
    class DeltaQueueAction *next;
    virtual void HandleTimeout(bool isPurge) = 0;
    unsigned m_delta;
    SessionDriver *m_driver;
};


#endif // _DELTAQUEUEACTION_HPP_INCLUDED_
