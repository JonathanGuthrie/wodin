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

#include "noophandler.hpp"

ImapHandler *noopHandler(ImapSession *session, INPUT_DATA_STRUCT &input) {
    (void) input;
    return new NoopHandler(session);
}

/*
 * The NOOP may be used by the server to trigger things like checking for
 * new messages although the server doesn't have to wait for this to do
 * things like that
 */
IMAP_RESULTS NoopHandler::receiveData(INPUT_DATA_STRUCT &input) {
    // This command literally doesn't do anything.  If there was an update, it was found
    // asynchronously and the updated info will be printed in formatTaggedResponse

    return IMAP_OK;
}
