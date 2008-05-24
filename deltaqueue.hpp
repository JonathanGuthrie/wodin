#if !defined(_DELTAQUEUE_HPP_INCLUDED_)
#define _DELTAQUEUE_HPP_INCLUDED_

#include "deltaqueueaction.hpp"

#include "string"

class SessionDriver;

class DeltaQueue
{
public:
    void Tick(void);
    void AddSend(SessionDriver *driver, unsigned seconds, const std::string &message);
    void AddTimeout(SessionDriver *driver, time_t timeout);
    void AddAsynchronousAction(SessionDriver *driver, time_t timeout);
    void AddRetry(SessionDriver *driver, time_t timeout);
    DeltaQueue();
    void InsertNewAction(DeltaQueueAction *action);
    void PurgeSession(const SessionDriver *driver);

private:
    pthread_mutex_t m_queueMutex;
    DeltaQueueAction *m_queueHead;
};

#endif // _DELTAQUEUE_HPP_INCLUDED_
