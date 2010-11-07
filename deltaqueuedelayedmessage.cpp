#include <clotho/internetserver.hpp>

#include "deltaqueuedelayedmessage.hpp"
#include "imapsession.hpp"

DeltaQueueDelayedMessage::DeltaQueueDelayedMessage(int delta, InternetSession *session, const std::string message) : DeltaQueueAction(delta, session), message(message) {}


void DeltaQueueDelayedMessage::HandleTimeout(bool isPurge) {
    if (!isPurge) {
	const ImapSession *imap_session = dynamic_cast<const ImapSession *>(m_session);
	imap_session->GetDriver()->WantsToSend(message);
	imap_session->GetDriver()->WantsToReceive();
    }
}
