/*
 * Copyright 2013 Jonathan R. Guthrie
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#if !defined(_DELTAQUEUEASYNCHRONOUSACTION_HPP_INCLUDED_)
#define _DELTAQUEUEASYNCHRONOUSACTION_HPP_INCLUDED_

#include <clotho/deltaqueueaction.hpp>

#include "mailstore.hpp"

// When the timer expires, execute the check for mailbox changes method in the selected
// mail store.
class DeltaQueueAsynchronousAction : public DeltaQueueAction {
public:
    DeltaQueueAsynchronousAction(int delta, InternetSession *session);
    virtual void handleTimeout(bool isPurge);
};


#endif // _DELTAQUEUEASYNCHRONOUSACTION_HPP_INCLUDED_
