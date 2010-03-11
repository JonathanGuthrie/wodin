#include "internetsession.hpp"
#include "servermaster.hpp"
#include "sessiondriver.hpp"

InternetSession::InternetSession(Socket *sock, ServerMaster *master, SessionDriver *driver) : m_s(sock), m_master(master), m_driver(driver) {
}


InternetSession::~InternetSession(void) {
}
