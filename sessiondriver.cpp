#include "sessiondriver.hpp"
#include "imapserver.hpp"

SessionDriver::SessionDriver(ImapServer *s, int pipe)
{
    this->m_pipe = pipe;
    m_server = s;
    m_sock = NULL;
    m_session = NULL;
    pthread_mutex_init(&m_workMutex, NULL);
}

SessionDriver::~SessionDriver()
{
    if (NULL != m_sock) {
	delete m_sock;
    }
    if (NULL != m_session) {
	delete m_session;
    }
}

void SessionDriver::DoWork(void)
{
    uint8_t recvBuffer[1000];
    // When it gets here, it knows that the receive on Sock will not block
    ssize_t numOctets = m_sock->Receive(recvBuffer, 1000);
    if (0 < numOctets)
    {
	pthread_mutex_lock(&m_workMutex);
	int result = m_session->ReceiveData(recvBuffer, numOctets);
	pthread_mutex_unlock(&m_workMutex);
	if (0 > result)
	{
	    m_server->KillSession(this);
	}
	else
	{
	    if (0 == result)
	    {
		m_server->WantsToReceive(m_sock->SockNum());
	    }
	}
    }
}


void SessionDriver::DoAsynchronousWork(void) {
    pthread_mutex_lock(&m_workMutex);
    m_session->AsynchronousEvent();
    pthread_mutex_unlock(&m_workMutex);
}


void SessionDriver::DoRetry(void) {
    uint8_t t[1];

    pthread_mutex_lock(&m_workMutex);
    int result = m_session->HandleOneLine(t, 0);
    pthread_mutex_unlock(&m_workMutex);
    if (0 > result)
    {
	m_server->KillSession(this);
    }
    else
    {
	if (0 == result)
	{
	    m_server->WantsToReceive(m_sock->SockNum());
	}
    }
}


void SessionDriver::DestroySession(void) {
    delete m_sock;
    m_sock = NULL;
    delete m_session;
    m_session = NULL;
}


void SessionDriver::NewSession(Socket *s)
{
    m_sock = s;
    m_session = new ImapSession(s, m_server, this, m_server->GetBadLoginPause());
}
