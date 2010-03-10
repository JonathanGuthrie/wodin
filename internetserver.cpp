#include <iostream>
#include <time.h>
#include <errno.h>

#include "internetserver.hpp"
#include "sessiondriver.hpp"
#include "servermaster.hpp"

InternetServer::InternetServer(uint32_t bind_address, short bind_port, ServerMaster *master, int num_workers) throw(ServerErrorException) {
  if (0 > pipe(m_pipeFd)) {
    throw ServerErrorException(errno);
  }
  m_workerCount = num_workers;
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


InternetServer::~InternetServer() {
}


void InternetServer::Run() {
  if (0 == pthread_create(&m_listenerThread, NULL, ListenerThreadFunction, this)) {
    if (0 == pthread_create(&m_receiverThread, NULL, ReceiverThreadFunction, this)) {
      if (0 == pthread_create(&m_timerQueueThread, NULL, TimerQueueFunction, this)) {
	m_pool = new WorkerPool(m_workerCount);
      }
    }
  }
}


void InternetServer::Shutdown() {
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


void *InternetServer::ListenerThreadFunction(void *d) {
  InternetServer *t = (InternetServer *)d;
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


void *InternetServer::ReceiverThreadFunction(void *d) {
  InternetServer *t = (InternetServer *)d;
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


void *InternetServer::TimerQueueFunction(void *d) {
  InternetServer *t = (InternetServer *)d;
  while(t->m_isRunning) {
    sleep(1);
    // puts("tick");
    t->m_timerQueue.Tick();
  }
  return NULL;
}


void InternetServer::WantsToReceive(int which) {
  pthread_mutex_lock(&m_masterFdMutex);
  FD_SET(which, &m_masterFdList);
  pthread_mutex_unlock(&m_masterFdMutex);
  ::write(m_pipeFd[1], "r", 1);
}


void InternetServer::KillSession(SessionDriver *driver) {
  m_timerQueue.PurgeSession(driver);
  pthread_mutex_lock(&m_masterFdMutex);
  FD_CLR(driver->GetSocket()->SockNum(), &m_masterFdList);
  pthread_mutex_unlock(&m_masterFdMutex);
  driver->DestroySession();
  ::write(m_pipeFd[1], "r", 1);
}


void InternetServer::AddTimerAction(DeltaQueueAction *action) {
  m_timerQueue.InsertNewAction(action);
}
