CC=g++
CXXFLAGS=-g
LDFLAGS=-lpthread -lcrypt -lcppserver

%.d: %.cpp
	@set -e; rm -f $@; \
	$(CC) -MM $(CPPFLAGS) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

SOURCES=imapd.cpp \
	imapsession.cpp \
	imapunixuser.cpp \
	imapuser.cpp \
	sasl.cpp \
	base64.cpp \
	mailstore.cpp \
	mailstorembox.cpp \
	deltaqueueidletimer.cpp \
	deltaqueueasynchronousaction.cpp \
	deltaqueuedelayedmessage.cpp \
	deltaqueueretry.cpp \
	namespace.cpp \
	mailstoreinvalid.cpp \
	datetime.cpp \
	mailsearch.cpp \
	mailmessage.cpp \
	mailstorelocktest.cpp \
	imapdriver.cpp \
	imapsessionfactory.cpp \
	imapmaster.cpp

imapd: imapd.o imapsession.o imapunixuser.o imapuser.o sasl.o base64.o mailstorembox.o \
        mailstore.o deltaqueueidletimer.o deltaqueueasynchronousaction.o deltaqueuedelayedmessage.o \
        namespace.o mailstoreinvalid.o datetime.o mailsearch.o mailmessage.o deltaqueueretry.o mailstorelocktest.o \
        imapdriver.o imapsessionfactory.o imapmaster.o

include $(SOURCES:.cpp=.d)

imapd.o:  Makefile

imapsession.o:  Makefile

socket.o:  Makefile

imapunixuser.o:  Makefile

imapuser.o:  Makefile

sasl.o:  Makefile

base64.o:  Makefile

mailstorembox.o:  Makefile

mailstore.o:  Makefile

deltaqueueidletimer.o:  Makefile

deltaqueuecheckmailbox.o:  Makefile

deltaqueuedelayedmessage.o: Makefile

namespace.o: Makefile

mailstoreinvalid.o: Makefile

datetime.o: Makefile

mailsearch.o: Makefile

mailstorelocktest.o:  Makefile

imapdriver.o: Makefile

imapsessionfactory.o: Makefile

imapmaster.o: Makefile

clean:
	rm -f *.o *.d imapd

