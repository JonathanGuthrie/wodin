#if !defined(_DELTAQUEUEACTION_HPP_INCLUDED_)
#define _DELTAQUEUEACTION_HPP_INCLUDED_

class SessionDriver;

class DeltaQueueAction
{
public:
    DeltaQueueAction(int delta, SessionDriver *driver);
    class DeltaQueueAction *next;
    virtual void HandleTimeout(bool isPurge) = 0;
    unsigned delta;
    SessionDriver *driver;
};


#endif // _DELTAQUEUEACTION_HPP_INCLUDED_
