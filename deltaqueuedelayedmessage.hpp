#if !defined(_DELTAQUEUEDELAYEDMESSAGE_HPP_INCLUDED_)
#define _DELTAQUEUEDELAYEDMESSAGE_HPP_INCLUDED_

#include <string>

#include <libcppserver/deltaqueueaction.hpp>

class SessionDriver;

class DeltaQueueDelayedMessage : public DeltaQueueAction {
public:
    DeltaQueueDelayedMessage(int delta, SessionDriver *driver, const std::string message); // Note:  Calling copy constructor on the message
    virtual void HandleTimeout(bool isPurge);

private:
    const std::string message;
};

#endif // _DELTAQUEUEDELAYEDMESSAGE_HPP_INCLUDED_
