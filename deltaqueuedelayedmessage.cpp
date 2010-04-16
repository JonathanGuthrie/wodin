#include <libcppserver/internetserver.hpp>

#include "deltaqueuedelayedmessage.hpp"
#include "imapsession.hpp"

DeltaQueueDelayedMessage::DeltaQueueDelayedMessage(int delta, SessionDriver *driver, const std::string message) : DeltaQueueAction(delta, driver), message(message) {}


void DeltaQueueDelayedMessage::HandleTimeout(bool isPurge) {
    if (!isPurge) {
	Socket *sock = m_driver->GetSocket();

	sock->Send((uint8_t *)message.data(), message.size());
	m_driver->GetServer()->WantsToReceive(sock->SockNum());
    }
}
