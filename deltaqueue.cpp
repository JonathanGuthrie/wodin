#include <stdlib.h>

#include "deltaqueue.hpp"
#include "deltaqueuedelayedmessage.hpp"
#include "deltaqueueasynchronousaction.hpp"
#include "deltaqueueidletimer.hpp"
#include "deltaqueueretry.hpp"

DeltaQueue::DeltaQueue() : m_queueHead(NULL) {
    pthread_mutex_init(&m_queueMutex, NULL);
}


// This function is structured kind of strangely, so a word of explanation is in
// order.  This program has multiple threads doing all kinds of things at the same
// time.  In order to prevent race conditions, delta queues are protected by
// a queueMutex.  However, that means that we have to be careful about taking stuff
// out of the queue whether by the timer expiring or by an as yet unsupported
// cancellation.  The obvious way of doing this is to call HandleTimeout in the 
// while loop that checks for those items that have timed out, however this is also
// a wrong way of doing it because HandleTimeout can (and often does) call InsertNewAction
// which also locks queueMutex, leading to deadlock.  One could put an unlock and a lock
// around the call to HandleTimeout in the while loop, but that would be another wrong
// way to do it because the queue might be (and often is) updated during the call to
// HandleTimeout, and the copy used by DeltaQueue might, as a result could be broken.

// The correct way is the way that I do it.  I collect all expired timers into a list and
// then run through the list calling HandleTimeout in a separate while loop after the exit
// from the critical section.  Since this list is local to this function and, therefore, to
// this thread, I don't have to protect it with a mutex, and since I've unlocked the mutex,
// the call to HandleTimeout can do anything it damn well pleases and there won't be a
// deadlock due to the code here.
void DeltaQueue::Tick() {
    DeltaQueueAction *temp = NULL;

    pthread_mutex_lock(&m_queueMutex);
    // decrement head
    if (NULL != m_queueHead) {
	--m_queueHead->m_delta;
	if (0 == m_queueHead->m_delta) {
	    DeltaQueueAction *endMarker;

	    temp = m_queueHead;
	    while ((NULL != m_queueHead) && (0 == m_queueHead->m_delta))
	    {
		endMarker = m_queueHead;
		m_queueHead = m_queueHead->next;
	    }
	    endMarker->next = NULL;
	}
    }
    pthread_mutex_unlock(&m_queueMutex);
    while (NULL != temp) {
	DeltaQueueAction *next = temp->next;

	temp->HandleTimeout(false);
	delete temp;
	temp = next;
    }
}


void DeltaQueue::InsertNewAction(DeltaQueueAction *action)
{
    pthread_mutex_lock(&m_queueMutex);
    if ((NULL == m_queueHead) || (m_queueHead->m_delta > action->m_delta))
    {
	if (NULL != m_queueHead)
	{
	    m_queueHead->m_delta -= action->m_delta;
	}
	action->next = m_queueHead;
	m_queueHead = action;
    }
    else
    {
	// If I get here, I know that the first item is not going to be the new action
	DeltaQueueAction *item = m_queueHead;

	action->m_delta -= m_queueHead->m_delta;
	for (item=m_queueHead; (item->next!=NULL) && (item->next->m_delta < action->m_delta); item=item->next)
	{
	    action->m_delta -= item->next->m_delta;
	}
	// When I get here, I know that item points to the item before where the new action goes
	if (NULL != item->next)
	{
	    item->next->m_delta -= action->m_delta;
	}
	action->next = item->next;
	item->next = action;
    }
    pthread_mutex_unlock(&m_queueMutex);
}


void DeltaQueue::PurgeSession(const SessionDriver *driver) {
    DeltaQueueAction *temp, *next;
    DeltaQueueAction *prev = NULL;
    DeltaQueueAction *purgeList = NULL;

    pthread_mutex_lock(&m_queueMutex);
    for(temp = m_queueHead; NULL != temp; temp=next) {
	next = temp->next;
	if (driver == temp->m_driver) {
	    if (NULL != next) {
		next->m_delta += temp->m_delta;
	    }
	    if (NULL != prev) {
		prev->next = temp->next;
	    }
	    else {
		m_queueHead = temp->next;
	    }
	    temp->next = purgeList;
	    purgeList = temp;
	}
	else {
	    prev = temp;
	}
    }
    pthread_mutex_unlock(&m_queueMutex);
    while (NULL != purgeList) {
	DeltaQueueAction *next = purgeList->next;

	purgeList->HandleTimeout(true);
	delete purgeList;
	purgeList = next;
    }
}
