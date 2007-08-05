#if !defined(THREADPOOL_H_INCLUDED)
#define THREADPOOL_H_INCLUDED

#include <queue>
#include <list>

#include <pthread.h>

class ThreadPoolException
{
};

// class MessageType must have a method called "void DoWork(void)"
template <typename T>
class ThreadPool
{
public:
    typedef typename std::list<T> Tlist;
    typedef typename std::queue<T, Tlist> Tqueue;
    typedef struct WorkerThreadInfo
    {
	ThreadPool *m_Instance;
	int m_QueueToUse;
    } WorkerThreadInfo;
    ThreadPool(int numThreads = 1, typename Tqueue::size_type queue_highwater = 0);
    ~ThreadPool(void);
    void SendMessage(T message);
    void SendMessages(Tqueue &messages);
    void SendMessages(Tlist &messages);
    void WorkerThread(int queue_number);
    static void *ThreadFunction(void*);

private:
    Tqueue *m_TheQueues;
    unsigned long m_QueueHighwater;
    pthread_mutex_t *m_QueueMutex;
    pthread_cond_t *m_QueueEmpty;
    pthread_cond_t *m_QueueFull;
    int m_WorkerThreadCount;
    int m_NextWorkerThread;
    pthread_mutex_t m_NextMutex;
    pthread_t *m_WorkerIds;
    WorkerThreadInfo *m_WorkerThreadParams;
    bool m_StopRunning;
};

// tf must be a thread function that looks like this.  I need it because I can't figure out how to put it in with the
// static void *worker_thread_function(void *instance_pointer)
// {
//    ThreadPool<TestHandler*>::WorkerThreadInfo *info = (ThreadPool<TestHandler*>::WorkerThreadInfo *)instance_pointer;
//    info->m_Instance->WorkerThread(info->m_QueueToUse);
//    return NULL;
// }

template <typename T>
void *ThreadPool<T>::ThreadFunction(void *instance_pointer)
{
    typename ThreadPool<T>::WorkerThreadInfo *info = (typename ThreadPool<T>::WorkerThreadInfo *)instance_pointer;
    info->m_Instance->WorkerThread(info->m_QueueToUse);
    return NULL;
}


template <typename T>
ThreadPool<T>::ThreadPool(int numThreads, typename ThreadPool<T>::Tqueue::size_type queue_highwater)
{
    m_QueueHighwater = queue_highwater;
    m_WorkerThreadCount = numThreads;
    pthread_mutex_init(&m_NextMutex, NULL);
    m_NextWorkerThread = 0;
    m_StopRunning = false;

    m_TheQueues = new typename ThreadPool<T>::Tqueue[m_WorkerThreadCount];

    m_QueueMutex = new pthread_mutex_t[m_WorkerThreadCount];
    m_QueueEmpty = new pthread_cond_t[m_WorkerThreadCount];
    m_QueueFull = new pthread_cond_t[m_WorkerThreadCount];

    for (int i=0; i<m_WorkerThreadCount; ++i)
    {
	pthread_mutex_init(&m_QueueMutex[i], NULL);
	pthread_cond_init(&m_QueueEmpty[i], NULL);
	pthread_cond_init(&m_QueueFull[i], NULL);
    }

    m_WorkerIds = new pthread_t[m_WorkerThreadCount];
    m_WorkerThreadParams = new WorkerThreadInfo[m_WorkerThreadCount];
    for (int i=0; i<m_WorkerThreadCount; ++i)
    {
	m_WorkerThreadParams[i].m_Instance = this;
	m_WorkerThreadParams[i].m_QueueToUse = i;
	if (0 != pthread_create(&m_WorkerIds[i], NULL, ThreadFunction, &m_WorkerThreadParams[i]))
	{
	    throw ThreadPoolException();
	}
    }
}

template <typename T>
ThreadPool<T>::~ThreadPool(void)
{
    m_StopRunning = true;
    for (int i=0; i<m_WorkerThreadCount; ++i)
    {
	pthread_cond_signal(&m_QueueFull[i]);
	pthread_join(m_WorkerIds[i], NULL);
    }

    for (int i=0; i<m_WorkerThreadCount; ++i)
    {
	pthread_cond_destroy(&m_QueueFull[i]);
	pthread_cond_destroy(&m_QueueEmpty[i]);
	pthread_mutex_destroy(&m_QueueMutex[i]);
    }

    delete[] m_WorkerThreadParams;
    delete[] m_WorkerIds;
    delete[] m_QueueFull;
    delete[] m_QueueEmpty;
    delete[] m_QueueMutex;
    delete[] m_TheQueues;
}

/*
 * This takes a single session and passes it to the destination.  If the queue is full (that is, queue_highwater
 * is nonzero and there are at least queue_highwater elements in the queue) then this will block until 
 * the queue is emptied by a call to receive messages, and then the element is added.
 */
template <typename T>
void ThreadPool<T>::SendMessage(T message)
{
    pthread_mutex_lock(&m_NextMutex);
    m_NextWorkerThread = (m_NextWorkerThread + 1) % m_WorkerThreadCount;
    int worker = m_NextWorkerThread;
    pthread_mutex_unlock(&m_NextMutex);

    // SYZYGY -- if a queue is too full, it should try the next until all of them have been tried
    pthread_mutex_lock(&m_QueueMutex[worker]);
    while ((0 != m_QueueHighwater) && (m_QueueHighwater < m_TheQueues[worker].size()))
    {
	pthread_cond_wait(&m_QueueEmpty[worker], &m_QueueMutex[worker]);
    }
    m_TheQueues[worker].push(message);
    pthread_mutex_unlock(&m_QueueMutex[worker]);
    pthread_cond_signal(&m_QueueFull[worker]);
}

/*
 * This takes queue of sessions and passes them to the destination.  If the queue is full (that is, queue_highwater
 * is nonzero and there are at least queue_highwater elements in the queue) then this will block until 
 * the queue is emptied by a call to receive messages, and then the elements are added.
 */
template <typename T>
void ThreadPool<T>::SendMessages(Tqueue &messages)
{
    pthread_mutex_lock(&m_NextMutex);
    m_NextWorkerThread = (m_NextWorkerThread + 1) % m_WorkerThreadCount;
    int worker = m_NextWorkerThread;
    pthread_mutex_unlock(&m_NextMutex);

    // SYZYGY -- the messages should be distributed across multiple queues
    // SYZYGY -- if a queue is too full, it should try the next until all of them have been tried
    pthread_mutex_lock(&m_QueueMutex[worker]);
    while ((0 != m_QueueHighwater) && (m_QueueHighwater < m_TheQueues[worker].size()))
    {
	pthread_cond_wait(&m_QueueEmpty[worker], &m_QueueMutex[worker]);
    }
    while(!messages.empty())
    {
	m_TheQueues[worker].push(messages.front());
	messages.pop();
    }
    pthread_mutex_unlock(&m_QueueMutex[worker]);
    pthread_cond_signal(&m_QueueFull[worker]);
}

/*
 * This takes a list of sessions and passes them to the destination.  If the queue is full (that is, queue_highwater
 * is nonzero and there are at least queue_highwater elements in the queue) then this will block until 
 * the queue is emptied by a call to receive messages, and then the elements are added.
 */
template <typename T>
void ThreadPool<T>::SendMessages(Tlist &messages)
{
    pthread_mutex_lock(&m_NextMutex);
    m_NextWorkerThread = (m_NextWorkerThread + 1) % m_WorkerThreadCount;
    int worker = m_NextWorkerThread;
    pthread_mutex_unlock(&m_NextMutex);

    // SYZYGY -- the messages should be distributed across multiple queues
    // SYZYGY -- if a queue is too full, it should try the next until all of them have been tried
    pthread_mutex_lock(&m_QueueMutex[worker]);
    while ((0 != m_QueueHighwater) && (m_QueueHighwater < m_TheQueues[worker].size()))
    {
	pthread_cond_wait(&m_QueueEmpty[worker], &m_QueueMutex[worker]);
    }
    for (typename ThreadPool<T>::Tlist::iterator i=messages.begin(); i!=messages.end(); ++i) {
	m_TheQueues[worker].push(*i);
    }
    pthread_mutex_unlock(&m_QueueMutex[worker]);
    pthread_cond_signal(&m_QueueFull[worker]);
}

template <typename T>
void ThreadPool<T>::WorkerThread(int queue_number)
{
    // std::cout << "In the worker thread method, I've got a listener for queue " << queue_number << std::endl;
    while(1)
    {
	pthread_mutex_lock(&m_QueueMutex[queue_number]);
	while(m_TheQueues[queue_number].empty())
	{
	    pthread_cond_wait(&m_QueueFull[queue_number], &m_QueueMutex[queue_number]);
	    if (m_StopRunning)
	    {
		break;
	    }
	}
	if (m_StopRunning)
	{
	    break;
	}
	while(!m_TheQueues[queue_number].empty())
	{
	    m_TheQueues[queue_number].front()->DoWork();
	    m_TheQueues[queue_number].pop();
	}
	pthread_mutex_unlock(&m_QueueMutex[queue_number]);
	pthread_cond_signal(&m_QueueEmpty[queue_number]);
    }
}

#endif // THREADPOOL_H_INCLUDED
