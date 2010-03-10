#include <iostream>
#include <time.h>
#include <errno.h>

#include "deltaqueue.hpp"

#include "imapserver.hpp"
#include "imapsession.hpp"

#include "sessiondriver.hpp"
#include "servermaster.hpp"

ImapServer::ImapServer(uint32_t bind_address, short bind_port, ServerMaster *master) throw(ServerErrorException) {
  if (0 > pipe(m_pipeFd)) {
    throw ServerErrorException(errno);
  }
  m_master = master;
  m_isRunning = true;
  m_listener = new Socket(bind_address, bind_port);
  for (int i=0; i<FD_SETSIZE; ++i) {
    m_sessions[i] = new SessionDriver(this, m_pipeFd[1], master);
  }
  FD_ZERO(&m_masterFdList);
  FD_SET(m_pipeFd[0], &m_masterFdList);
  pthread_mutex_init(&m_masterFdMutex, NULL);
}


ImapServer::~ImapServer() {
}


void ImapServer::Run() {
  if (0 == pthread_create(&m_listenerThread, NULL, ListenerThreadFunction, this)) {
    if (0 == pthread_create(&m_receiverThread, NULL, ReceiverThreadFunction, this)) {
	  if (0 == pthread_create(&m_timerQueueThread, NULL, TimerQueueFunction, this)) {
	      m_pool = new ImapWorkerPool(1);
	    }
	}
    }
}


void ImapServer::Shutdown() {
  m_isRunning = false;
  pthread_cancel(m_listenerThread);
  pthread_join(m_listenerThread, NULL);
  delete m_listener;
  m_listener = NULL;
  ::write(m_pipeFd[1], "q", 1);
  pthread_join(m_receiverThread, NULL);
  pthread_join(m_timerQueueThread, NULL);

  std::string bye = "* BYE Server Shutting Down\r\n";
  for (int i=0; i<FD_SETSIZE; ++i) {
      Socket *s;
      if (NULL != (s = m_sessions[i]->GetSocket())) {
	s->Send((uint8_t*)bye.data(), bye.size());
      }
      delete m_sessions[i];
    }
  delete m_pool;
}


void *ImapServer::ListenerThreadFunction(void *d) {
  ImapServer *t = (ImapServer *)d;
  while(t->m_isRunning) {
      Socket *worker = t->m_listener->Accept();
      if (FD_SETSIZE > worker->SockNum()) {
	  t->m_sessions[worker->SockNum()]->NewSession(worker);
	}
      else {
	  delete worker;
	}
    }
  return NULL;
}


void *ImapServer::ReceiverThreadFunction(void *d) {
  ImapServer *t = (ImapServer *)d;
  while(t->m_isRunning) {
      int maxFd;
      fd_set localFdList;
      pthread_mutex_lock(&t->m_masterFdMutex);
      localFdList = t->m_masterFdList;
      pthread_mutex_unlock(&t->m_masterFdMutex);
      for (int i=t->m_pipeFd[0]; i<FD_SETSIZE; ++i) {
	  if (FD_ISSET(i, &localFdList)) {
	      maxFd = i+1;
	    }
	}
      if (0 < select(maxFd, &localFdList, NULL, NULL, NULL)) {
	  for (int i=0; i<FD_SETSIZE; ++i) {
	      if (FD_ISSET(i, &localFdList)) {
		  if (i != t->m_pipeFd[0]) {
		      t->m_pool->SendMessage(t->m_sessions[i]);
		      pthread_mutex_lock(&t->m_masterFdMutex);
		      FD_CLR(i, &t->m_masterFdList);
		      pthread_mutex_unlock(&t->m_masterFdMutex);
		    }	
		  else {
		      char c;
		      ::read(t->m_pipeFd[0], &c, 1);
		    }
		}
	    }
	}
    }
  return NULL;
}


void *ImapServer::TimerQueueFunction(void *d) {
  ImapServer *t = (ImapServer *)d;
  while(t->m_isRunning) {
    sleep(1);
    // puts("tick");
    t->m_master->Tick();
  }
  return NULL;
}


void ImapServer::WantsToReceive(int which) {
  pthread_mutex_lock(&m_masterFdMutex);
  FD_SET(which, &m_masterFdList);
  pthread_mutex_unlock(&m_masterFdMutex);
  ::write(m_pipeFd[1], "r", 1);
}


void ImapServer::KillSession(SessionDriver *driver) {
  m_master->PurgeTimer(driver);
  pthread_mutex_lock(&m_masterFdMutex);
  FD_CLR(driver->GetSocket()->SockNum(), &m_masterFdList);
  pthread_mutex_unlock(&m_masterFdMutex);
  driver->DestroySession();
  ::write(m_pipeFd[1], "r", 1);
}
