#if !defined(_DELTAQUEUE_HPP_INCLUDED_)
#define _DELTAQUEUE_HPP_INCLUDED_

#include "deltaqueueaction.hpp"

#include "string"

class SessionDriver;

class DeltaQueue
{
public:
    void Tick(void);
    DeltaQueue();
    void InsertNewAction(DeltaQueueAction *action);
    void PurgeSession(const SessionDriver *driver);

private:
    pthread_mutex_t m_queueMutex;
    DeltaQueueAction *m_queueHead;
};

#endif // _DELTAQUEUE_HPP_INCLUDED_
