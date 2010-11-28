CC=g++
CXXFLAGS=-g
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

locking-test: locking-test.o imapsession.o

include $(SOURCES:.cpp=.d)

clean:
	rm -f *.o *.d imapd locking-test

