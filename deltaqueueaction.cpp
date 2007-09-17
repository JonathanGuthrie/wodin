#include <stdlib.h>

#include "deltaqueueaction.hpp"

DeltaQueueAction::DeltaQueueAction(int delta, SessionDriver *driver) : m_delta(delta), m_driver(driver), next(NULL)
{
}


