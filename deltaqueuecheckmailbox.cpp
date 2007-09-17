#include <time.h>

#include "deltaqueuecheckmailbox.hpp"
#include "deltaqueueaction.hpp"
#include "imapserver.hpp"
#include "imapsession.hpp"

DeltaQueueCheckMailbox::DeltaQueueCheckMailbox(int delta, SessionDriver *driver, MailStore *store) : DeltaQueueAction(delta, driver), m_store(store) { }


void DeltaQueueCheckMailbox::HandleTimeout(bool isPurge)
{
    if (!isPurge) {
#if 0 // SYZYGY
	m_store->MailboxChecknew();
#endif // 0
    }
}
