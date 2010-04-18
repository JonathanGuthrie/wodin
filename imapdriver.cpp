#include <libcppserver/internetserver.hpp>

#include "imapdriver.hpp"
#include "imapmaster.hpp"
#include "imapsession.hpp"

ImapDriver::ImapDriver(InternetServer *s, ServerMaster *master) : SessionDriver(s, master) {
  m_master = master;
  m_server = s;
  m_sock = NULL;
  m_session = NULL;
  pthread_mutex_init(&m_workMutex, NULL);
}

ImapDriver::~ImapDriver() {
  if (NULL != m_sock) {
    delete m_sock;
  }
  if (NULL != m_session) {
    delete m_session;
  }
}

void ImapDriver::DoWork(void) {
  uint8_t recvBuffer[1000];
  // When it gets here, it knows that the receive on Sock will not block
  ssize_t numOctets = m_sock->Receive(recvBuffer, 1000);
  if (0 < numOctets) {
    ImapSession *imap_session = dynamic_cast<ImapSession *>(m_session);
    pthread_mutex_lock(&m_workMutex);
    int result = imap_session->ReceiveData(recvBuffer, numOctets);
    pthread_mutex_unlock(&m_workMutex);
    if (0 > result) {
      m_server->KillSession(this);
    }
    else {
      if (0 == result) {
	m_server->WantsToReceive(m_sock->SockNum());
      }
    }
  }
}


void ImapDriver::DoAsynchronousWork(void) {
  pthread_mutex_lock(&m_workMutex);
  ImapSession *imap_session = dynamic_cast<ImapSession *>(m_session);
  imap_session->AsynchronousEvent();
  pthread_mutex_unlock(&m_workMutex);
}


void ImapDriver::DoRetry(void) {
  uint8_t t[1];

  pthread_mutex_lock(&m_workMutex);
  ImapSession *imap_session = dynamic_cast<ImapSession *>(m_session);
  int result = imap_session->HandleOneLine(t, 0);
  pthread_mutex_unlock(&m_workMutex);
  if (0 > result) {
    m_server->KillSession(this);
  }
  else {
    if (0 == result) {
      m_server->WantsToReceive(m_sock->SockNum());
    }
  }
}


void ImapDriver::DestroySession(void) {
  delete m_sock;
  m_sock = NULL;
  delete m_session;
  m_session = NULL;
}


void ImapDriver::NewSession(Socket *s) {
  m_sock = s;
  ImapMaster *imap_master = dynamic_cast<ImapMaster *>(m_master);

  m_session = new ImapSession(s, imap_master, this);
  imap_master->SetIdleTimer(this, imap_master->GetLoginTimeout());
  imap_master->ScheduleAsynchronousAction(this, imap_master->GetAsynchronousEventTime());
  m_server->WantsToReceive(m_sock->SockNum());
}
