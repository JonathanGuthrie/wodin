#include <stdlib.h>

#include "deltaqueueaction.hpp"

DeltaQueueAction::DeltaQueueAction(int delta, SessionDriver *driver) : next(NULL), m_delta(delta), m_driver(driver)
{
}


