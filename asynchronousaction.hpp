#if !defined(_DELTAQUEUEASYNCHRONOUSACTION_HPP_INCLUDED_)
#define _DELTAQUEUEASYNCHRONOUSACTION_HPP_INCLUDED_

#include <clotho/deltaqueueaction.hpp>

#include "mailstore.hpp"

// When the timer expires, execute the check for mailbox changes method in the selected
// mail store.
class DeltaQueueAsynchronousAction : public DeltaQueueAction
{
public:
  DeltaQueueAsynchronousAction(int delta, InternetSession *session);
  virtual void HandleTimeout(bool isPurge);
};


#endif // _DELTAQUEUEASYNCHRONOUSACTION_HPP_INCLUDED_
