#include <iostream>
#include <time.h>

#include "imapserver.hpp"
#include "imapsession.hpp"

#include "imapunixuser.hpp"
#include "mailstorembox.hpp"

// The SessionDriver class sits between the server, which does the listening
// for more data, and the ImapSession, which does all the processing of the
// data.  It knows server-specific stuff and the merest bit of session stuff.
// This has to be a separate class from ImapServer because I want one of these
// for each ImapSession
class SessionDriver
{
public:
    SessionDriver(ImapServer *s, int pipe);
    ~SessionDriver();
    void DoWork(void);
    void NewSession(Socket *s);
    const ImapSession *GetSession(void) const { return session; }
    void DestroySession(void);

private:
    ImapSession *session;
    ImapServer *server;
    Socket *sock;
    int pipe;
};

SessionDriver::SessionDriver(ImapServer *s, int pipe)
{
    this->pipe = pipe;
    server = s;
}

SessionDriver::~SessionDriver()
{
    delete sock;
    delete session;
}

void SessionDriver::DoWork(void)
{
    uint8_t recvBuffer[1000];
    // When it gets here, it knows that the receive on Sock will not block
    ssize_t numOctets = sock->Receive(recvBuffer, 1000);
    if (0 < numOctets)
    {
	int result = session->ReceiveData(recvBuffer, numOctets);
	if (0 > result)
	{
	    DestroySession();
	}
	else
	{
	    if (0 == result)
	    {
		server->WantsToReceive(sock->SockNum());
	    }
	}
    }
}


void SessionDriver::DestroySession(void)
{
    delete sock;
    sock = NULL;
    delete session;
}


void SessionDriver::NewSession(Socket *s)
{
    sock = s;
    session = new ImapSession(s, server, 5);
}


ImapServer::ImapServer(uint32_t bind_address, short bind_port, unsigned login_timeout, unsigned idle_timeout)
{
    loginTimeout = login_timeout;
    idleTimeout = idle_timeout;
    pipe(pipeFd);  // SYZYGY exception on error
    isRunning = true;
    listener = new Socket(bind_address, bind_port);
    ImapSession::BuildSymbolTables();
    for (int i=0; i<FD_SETSIZE; ++i)
    {
	sessions[i] = new SessionDriver(this, pipeFd[1]);
    }
    FD_ZERO(&masterFdList);
    FD_SET(pipeFd[0], &masterFdList);
    pthread_mutex_init(&masterFdMutex, NULL);
}


ImapServer::~ImapServer()
{
}


void ImapServer::Run()
{
    if (0 == pthread_create(&listenerThread, NULL, ListenerThreadFunction, this)) 
    {
	if (0 == pthread_create(&receiverThread, NULL, ReceiverThreadFunction, this)) 
	{
	    if (0 == pthread_create(&timerQueueThread, NULL, TimerQueueFunction, this))
	    {
		pool = new ImapWorkerPool(1);
	    }
	}
    }
}


void ImapServer::Shutdown()
{
    // SYZYGY -- shutdown should send a "BYE" message to all attached clients
    isRunning = false;
    pthread_cancel(listenerThread);
    pthread_join(listenerThread, NULL);
    delete listener;
    listener = NULL;
    ::write(pipeFd[1], "q", 1);
    pthread_join(receiverThread, NULL);
    pthread_join(timerQueueThread, NULL);
}


void *ImapServer::ListenerThreadFunction(void *d)
{
    ImapServer *t = (ImapServer *)d;
    while(t->isRunning)
    {
	Socket *worker = t->listener->Accept();
	if (FD_SETSIZE > worker->SockNum())
	{
	    t->WantsToReceive(worker->SockNum());
	    t->sessions[worker->SockNum()]->NewSession(worker);
	    t->SetIdleTimer(t->sessions[worker->SockNum()]->GetSession(), t->loginTimeout);
	}
	else
	{
	    delete worker;
	}
    }
    return NULL;
}


void *ImapServer::ReceiverThreadFunction(void *d)
{
    ImapServer *t = (ImapServer *)d;
    while(t->isRunning)
    {
	int maxFd;
	fd_set localFdList;
	pthread_mutex_lock(&t->masterFdMutex);
	localFdList = t->masterFdList;
	pthread_mutex_unlock(&t->masterFdMutex);
	for (int i=t->pipeFd[0]; i<FD_SETSIZE; ++i)
	{
	    if (FD_ISSET(i, &localFdList))
	    {
		maxFd = i+1;
	    }
	}
	if (0 < select(maxFd, &localFdList, NULL, NULL, NULL))
	{
	    for (int i=0; i<FD_SETSIZE; ++i)
	    {
		if (FD_ISSET(i, &localFdList))
		{
		    if (i != t->pipeFd[0])
		    {
			t->pool->SendMessage(t->sessions[i]);
			pthread_mutex_lock(&t->masterFdMutex);
			FD_CLR(i, &t->masterFdList);
			pthread_mutex_unlock(&t->masterFdMutex);
		    }	
		    else
		    {
			char c;
			::read(t->pipeFd[0], &c, 1);
		    }
		}
	    }
	}
    }
    return NULL;
}


void *ImapServer::TimerQueueFunction(void *d)
{
    ImapServer *t = (ImapServer *)d;
    while(t->isRunning)
    {
	sleep(1);
	// puts("tick");
	t->timerQueue.Tick();
    }
    return NULL;
}


void ImapServer::WantsToReceive(int which)
{
    pthread_mutex_lock(&masterFdMutex);
    FD_SET(which, &masterFdList);
    pthread_mutex_unlock(&masterFdMutex);
    ::write(pipeFd[1], "r", 1);
}


void ImapServer::KillSession(const ImapSession *session)
{
    pthread_mutex_lock(&masterFdMutex);
    FD_SET(session->GetSocket()->SockNum(), &masterFdList);
    pthread_mutex_unlock(&masterFdMutex);
    sessions[session->GetSocket()->SockNum()]->DestroySession();
    ::write(pipeFd[1], "r", 1);
}


ImapUser *ImapServer::GetUserInfo(const char *userid)
{
    return (ImapUser *) new ImapUnixUser(userid);
}

MailStore *ImapServer::GetMailStore(const ImapUser *user)
{
    std::string inbox_dir = "/var/mail/";
    inbox_dir += user->GetName();
    // std::cout << "The user's home directory is \"" << user->GetHomeDir() << "\"" << std::endl;
    // std::cout << "The user's mail directory is \"" << inbox_dir << "\"" << std::endl;
    // return (MailStore *) new MailStoreMbox(inbox_dir.c_str(), user->GetHomeDir());  // SYZYGY -- set the INBOX directory based on some default value
    return (MailStore *) new MailStoreMbox("/var/mail/jguthrie", user->GetHomeDir());  // SYZYGY -- set the INBOX directory based on some default value
}


void ImapServer::DelaySend(const ImapSession *session, unsigned seconds, const std::string &message)
{
    timerQueue.AddSend(session, seconds, message);
}


void ImapServer::SetIdleTimer(const ImapSession *session, unsigned seconds)
{
    timerQueue.AddTimeout(session, seconds);
}


DeltaQueueAction::DeltaQueueAction(int delta) : delta(delta), next(NULL)
{
}


DeltaQueueIdleTimer::DeltaQueueIdleTimer(int delta, const ImapSession *session) : DeltaQueueAction(delta), session(session)
{
}


void DeltaQueueIdleTimer::HandleTimeout(bool isPurge)
{
    time_t now = time(NULL);
    unsigned timeout = (ImapNotAuthenticated == session->GetState()) ? session->GetServer()->GetLoginTimeout() : session->GetServer()->GetIdleTimeout();

    if ((now - timeout) > session->GetLastCommandTime())
    {
	// SYZYGY -- tell the system "bye" and hang up
	std::string bye = "* BYE Idle timeout disconnect\r\n";
	session->GetSocket()->Send((uint8_t *)bye.data(), bye.size());
	session->GetServer()->KillSession(session);
    }
    else
    {
	session->GetServer()->SetIdleTimer(session, (time_t) session->GetLastCommandTime() + timeout + 1 - now);
    }
}




DeltaQueueDelayedMessage::DeltaQueueDelayedMessage(int delta, const ImapSession *session, const std::string message) : DeltaQueueAction(delta), session(session), message(message)
{
}


void DeltaQueueDelayedMessage::HandleTimeout(bool isPurge) {
    Socket *sock = session->GetSocket();

    sock->Send((uint8_t *)message.data(), message.size());
    session->GetServer()->WantsToReceive(sock->SockNum());
}


DeltaQueue::DeltaQueue() : queueHead(NULL) {
    pthread_mutex_init(&queueMutex, NULL);
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

    pthread_mutex_lock(&queueMutex);
    // decrement head
    if (NULL != queueHead) {
	--queueHead->delta;
	if (0 == queueHead->delta) {
	    DeltaQueueAction *endMarker;

	    temp = queueHead;
	    while ((NULL != queueHead) && (0 == queueHead->delta))
	    {
		endMarker = queueHead;
		queueHead = queueHead->next;
	    }
	    endMarker->next = NULL;
	}
    }
    pthread_mutex_unlock(&queueMutex);
    while (NULL != temp) {
	DeltaQueueAction *next = temp->next;

	temp->HandleTimeout(false);
	delete temp;
	temp = next;
    }
}


void DeltaQueue::AddSend(const ImapSession *session, unsigned seconds, const std::string &message)
{
    DeltaQueueDelayedMessage *action;

    action = new DeltaQueueDelayedMessage(seconds, session, message);
    InsertNewAction((DeltaQueueAction *)action);
}


void DeltaQueue::AddTimeout(const ImapSession *session, time_t timeout)
{
    DeltaQueueIdleTimer *action;

    action = new DeltaQueueIdleTimer(timeout, session);
    InsertNewAction((DeltaQueueAction *)action);
}


void DeltaQueue::InsertNewAction(DeltaQueueAction *action)
{
    pthread_mutex_lock(&queueMutex);
    if ((NULL == queueHead) || (queueHead->delta > action->delta))
    {
	if (NULL != queueHead)
	{
	    queueHead->delta -= action->delta;
	}
	action->next = queueHead;
	queueHead = action;
    }
    else
    {
	// If I get here, I know that the first item is not going to be the new action
	DeltaQueueAction *item = queueHead;

	action->delta -= queueHead->delta;
	for (item=queueHead; (item->next!=NULL) && (item->next->delta < action->delta); item=item->next)
	{
	    action->delta -= item->next->delta;
	}
	// When I get here, I know that item points to the item before where the new action goes
	if (NULL != item->next)
	{
	    item->next->delta -= action->delta;
	}
	action->next = item->next;
	item->next = action;
    }
    pthread_mutex_unlock(&queueMutex);
}
