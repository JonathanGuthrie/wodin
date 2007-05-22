#include <stdlib.h>

#include "deltaqueueaction.hpp"

DeltaQueueAction::DeltaQueueAction(int delta, SessionDriver *driver) : delta(delta), driver(driver), next(NULL)
{
}


