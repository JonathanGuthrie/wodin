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

#include <time.h>

#include <clotho/deltaqueueaction.hpp>

#include "asynchronousaction.hpp"
#include "imapmaster.hpp"
#include "imapsession.hpp"


DeltaQueueAsynchronousAction::DeltaQueueAsynchronousAction(int delta, InternetSession *session) : DeltaQueueAction(delta, session) {
}


void DeltaQueueAsynchronousAction::handleTimeout(bool isPurge) {
#if !defined(TEST)
  ImapSession *imap_session = dynamic_cast<ImapSession *>(m_session);
  ImapMaster *imap_master = dynamic_cast<ImapMaster *>(imap_session->master());
  if (!isPurge) {
    imap_session->asynchronousEvent();
    imap_session->server()->addTimerAction(new DeltaQueueAsynchronousAction(imap_master->asynchronousEventTime(), m_session));
    }
#endif // !defined(TEST)
}
