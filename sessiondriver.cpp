#include "sessiondriver.hpp"
#include "internetserver.hpp"
#include "servermaster.hpp"

SessionDriver::SessionDriver(InternetServer *s, int pipe, ServerMaster *master) : m_server(s), m_pipe(pipe), m_master(master) {
}

SessionDriver::~SessionDriver(void) {
}
