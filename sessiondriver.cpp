#include "sessiondriver.hpp"
#include "internetserver.hpp"
#include "servermaster.hpp"

SessionDriver::SessionDriver(InternetServer *s, ServerMaster *master) : m_server(s), m_master(master) {
}

SessionDriver::~SessionDriver(void) {
}
