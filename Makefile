#
# Copyright 2013 Jonathan R. Guthrie
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

CC=g++
CXXFLAGS=-g
CXXTESTFLAGS=$(CXXFLAGS) -DTEST
LDFLAGS=-lpthread -lcrypt -lclotho -lboost_system

%.d: %.cpp
	@set -e; rm -f $@; \
	$(CC) -MM $(CPPFLAGS) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : Makefile ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

SOURCES=imapd.cpp \
	imapsession.cpp \
	imapunixuser.cpp \
	imapuser.cpp \
	sasl.cpp \
	base64.cpp \
	mailstore.cpp \
	mailstorembox.cpp \
	idletimer.cpp \
	asynchronousaction.cpp \
	delayedmessage.cpp \
	retry.cpp \
	namespace.cpp \
	mailstoreinvalid.cpp \
	datetime.cpp \
	mailsearch.cpp \
	mailmessage.cpp \
	imapmaster.cpp \
	locking-test.cpp \
	mailstorelocktest.cpp \
	imapmasterlocktest.cpp \
	locktestmaster.cpp \
	imaplocktestuser.cpp \
	unimplementedhandler.cpp \
	capabilityhandler.cpp \
	noophandler.cpp \
	logouthandler.cpp \
	authenticatehandler.cpp \
	loginhandler.cpp \
	namespacehandler.cpp \
	createhandler.cpp \
	deletehandler.cpp \
	renamehandler.cpp \
	subscribehandler.cpp \
	listhandler.cpp \
	statushandler.cpp \
	appendhandler.cpp \
	selecthandler.cpp \
	checkhandler.cpp \
	expungehandler.cpp \
	copyhandler.cpp \
	closehandler.cpp \
	searchhandler.cpp \
	fetchhandler.cpp \
	storehandler.cpp

imapd: imapd.o imapsession.o imapunixuser.o imapuser.o sasl.o base64.o mailstorembox.o \
        mailstore.o idletimer.o asynchronousaction.o delayedmessage.o \
        namespace.o mailstoreinvalid.o datetime.o mailsearch.o mailmessage.o retry.o \
	imapmaster.o unimplementedhandler.o capabilityhandler.o noophandler.o \
	logouthandler.o authenticatehandler.o loginhandler.o namespacehandler.o \
	createhandler.o deletehandler.o renamehandler.o subscribehandler.o \
	listhandler.o statushandler.o appendhandler.o selecthandler.o checkhandler.o \
	expungehandler.o copyhandler.o closehandler.o searchhandler.o fetchhandler.o \
	storehandler.o

testidletimer.o:  idletimer.cpp Makefile
	$(CC) $(CXXTESTFLAGS) -c -o $@ $<

testasynchronousaction.o:  asynchronousaction.cpp Makefile
	$(CC) $(CXXTESTFLAGS) -c -o $@ $<

# The first five targets are custom just for the test.  The imapmasterlocktest.cpp file contains a reimplementation of ImapMaster
locking-test: locking-test.o mailstorelocktest.o imapmasterlocktest.o locktestmaster.o \
	imaplocktestuser.o testidletimer.o testasynchronousaction.o namespace.o \
	mailstore.o imapsession.o datetime.o mailmessage.o delayedmessage.o retry.o \
	sasl.o mailsearch.o imapuser.o base64.o unimplementedhandler.o \
	capabilityhandler.o noophandler.o logouthandler.o authenticatehandler.o \
	loginhandler.o namespacehandler.o createhandler.o deletehandler.o renamehandler.o \
	 subscribehandler.o listhandler.o statushandler.o appendhandler.o selecthandler.o \
	checkhandler.o expungehandler.o copyhandler.o closehandler.o searchhandler.o \
	fetchhandler.o storehandler.o

include $(SOURCES:.cpp=.d)

clean:
	rm -f *.o *.d imapd locking-test

