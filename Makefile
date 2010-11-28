CC=g++
CXXFLAGS=-g
CXXTESTFLAGS=$(CXXFLAGS) -DTEST
LDFLAGS=-lpthread -lcrypt -lclotho

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
	mailstorelocktest.cpp \
	imapmaster.cpp

imapd: imapd.o imapsession.o imapunixuser.o imapuser.o sasl.o base64.o mailstorembox.o \
        mailstore.o idletimer.o asynchronousaction.o delayedmessage.o \
        namespace.o mailstoreinvalid.o datetime.o mailsearch.o mailmessage.o retry.o \
	imapmaster.o

testidletimer.o:  idletimer.cpp Makefile
	$(CC) $(CXXTESTFLAGS) -c -o $@ $<

testasynchronousaction.o:  asynchronousaction.cpp Makefile
	$(CC) $(CXXTESTFLAGS) -c -o $@ $<

# The first five targets are custom just for the test.  The imapmasterlocktest.cpp file contains a reimplementation of ImapMaster
locking-test: locking-test.o mailstorelocktest.o imapmasterlocktest.o imaplocktestuser.o  testidletimer.o testasynchronousaction.o namespace.o mailstore.o imapsession.o datetime.o mailmessage.o delayedmessage.o retry.o sasl.o mailsearch.o imapuser.o base64.o

include $(SOURCES:.cpp=.d)

clean:
	rm -f *.o *.d imapd locking-test

