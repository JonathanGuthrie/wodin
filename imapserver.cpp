#include <iostream>
#include <time.h>
#include <errno.h>

#include "deltaqueue.hpp"

#include "imapserver.hpp"
#include "imapsession.hpp"

#include "imapunixuser.hpp"
#include "mailstorembox.hpp"
#include "mailstoreinvalid.hpp"
#include "namespace.hpp"

SessionDriver::SessionDriver(ImapServer *s, int pipe)
{
    this->pipe = pipe;
    server = s;
    sock = NULL;
    session = NULL;
    pthread_mutex_init(&workMutex, NULL);
}

SessionDriver::~SessionDriver()
{
    if (NULL != sock) {
	delete sock;
    }
    if (NULL != session) {
	delete session;
    }
}

void SessionDriver::DoWork(void)
{
    uint8_t recvBuffer[1000];
    // When it gets here, it knows that the receive on Sock will not block
    ssize_t numOctets = sock->Receive(recvBuffer, 1000);
    if (0 < numOctets)
    {
	pthread_mutex_lock(&workMutex);
	int result = session->ReceiveData(recvBuffer, numOctets);
	pthread_mutex_unlock(&workMutex);
	if (0 > result)
	{
	    server->KillSession(this);
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


void SessionDriver::DoAsynchronousWork(void) {
    pthread_mutex_lock(&workMutex);
    session->AsynchronousEvent();
    pthread_mutex_unlock(&workMutex);
}


void SessionDriver::DestroySession(void) {
    delete sock;
    sock = NULL;
    delete session;
    session = NULL;
}


void SessionDriver::NewSession(Socket *s)
{
    sock = s;
    session = new ImapSession(s, server, this, server->GetBadLoginPause());
}


ImapServer::ImapServer(uint32_t bind_address, short bind_port, std::string fqdn, unsigned login_timeout, unsigned idle_timeout, unsigned asynchronous_event_time, unsigned bad_login_pause) throw(ServerErrorException)
{
    m_useConfiguredUid = false;
    m_configuredUid = 0;
    m_useConfiguredGid = false;
    m_configuredGid = 0;
    loginTimeout = login_timeout;
    idleTimeout = idle_timeout;
    asynchronousEventTime = asynchronous_event_time;
    m_badLoginPause = bad_login_pause;
    if (0 > pipe(pipeFd)) {
	throw ServerErrorException(errno);
    }
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
    isRunning = false;
    pthread_cancel(listenerThread);
    pthread_join(listenerThread, NULL);
    delete listener;
    listener = NULL;
    ::write(pipeFd[1], "q", 1);
    pthread_join(receiverThread, NULL);
    pthread_join(timerQueueThread, NULL);

    std::string bye = "* BYE Server Shutting Down\r\n";
    for (int i=0; i<FD_SETSIZE; ++i)
    {
	Socket *s;
	if (NULL != (s = sessions[i]->GetSocket())) {
	    s->Send((uint8_t*)bye.data(), bye.size());
	}
	delete sessions[i];
    }
    delete pool;
}


void *ImapServer::ListenerThreadFunction(void *d)
{
    ImapServer *t = (ImapServer *)d;
    while(t->isRunning)
    {
	Socket *worker = t->listener->Accept();
	if (FD_SETSIZE > worker->SockNum())
	{
	    t->sessions[worker->SockNum()]->NewSession(worker);
	    t->SetIdleTimer(t->sessions[worker->SockNum()], t->loginTimeout);
	    t->ScheduleAsynchronousAction(t->sessions[worker->SockNum()], t->asynchronousEventTime);
	    t->WantsToReceive(worker->SockNum());
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


void ImapServer::KillSession(SessionDriver *driver)
{
    timerQueue.PurgeSession(driver);
    pthread_mutex_lock(&masterFdMutex);
    FD_CLR(driver->GetSocket()->SockNum(), &masterFdList);
    pthread_mutex_unlock(&masterFdMutex);
    driver->DestroySession();
    ::write(pipeFd[1], "r", 1);
}


ImapUser *ImapServer::GetUserInfo(const char *userid)
{
    return (ImapUser *) new ImapUnixUser(userid, this);
}

Namespace *ImapServer::GetMailStore(ImapSession *session)
{
    Namespace *result = new Namespace(session);
    // need namespaces for #mhinbox, #mh, ~, #shared, #ftp, #news, and #public, just like
    // uw-imap, although all of them have stubbed-out mail stores for them

    result->AddNamespace(Namespace::PERSONAL, "", (MailStore *) new MailStoreMbox(session), '/');
    result->AddNamespace(Namespace::PERSONAL, "#mhinbox", (MailStore *) new MailStoreInvalid(session));
    result->AddNamespace(Namespace::PERSONAL, "#mh/", (MailStore *) new MailStoreInvalid(session), '/');
    result->AddNamespace(Namespace::OTHERS, "~", (MailStore *) new MailStoreInvalid(session), '/');
    result->AddNamespace(Namespace::SHARED, "#shared/", (MailStore *) new MailStoreInvalid(session), '/');
    result->AddNamespace(Namespace::SHARED, "#ftp/", (MailStore *) new MailStoreInvalid(session), '/');
    result->AddNamespace(Namespace::SHARED, "#news.", (MailStore *) new MailStoreInvalid(session), '.');
    result->AddNamespace(Namespace::SHARED, "#public/", (MailStore *) new MailStoreInvalid(session), '/');

    return result;
}


void ImapServer::DelaySend(SessionDriver *driver, unsigned seconds, const std::string &message)
{
    timerQueue.AddSend(driver, seconds, message);
}


void ImapServer::SetIdleTimer(SessionDriver *driver, unsigned seconds)
{
    timerQueue.AddTimeout(driver, seconds);
}


void ImapServer::ScheduleAsynchronousAction(SessionDriver *driver, unsigned seconds) {
    timerQueue.AddAsynchronousAction(driver, seconds);
}
