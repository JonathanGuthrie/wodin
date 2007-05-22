CC=g++
CXXFLAGS=-g
LDFLAGS=-lpthread -lcrypt

%.d: %.cpp
	@set -e; rm -f $@; \
	$(CC) -MM $(CPPFLAGS) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

SOURCES=imapd.cpp \
	imapsession.cpp \
	socket.cpp \
	imapserver.cpp \
	imapunixuser.cpp \
	imapuser.cpp \
	sasl.cpp \
	base64.cpp \
	mailstore.cpp \
	mailstorembox.cpp \
	deltaqueue.cpp \
	deltaqueueidletimer.cpp \
	deltaqueuedelayedmessage.cpp \
	deltaqueueaction.cpp

imapd: imapd.o imapserver.o imapsession.o socket.o imapunixuser.o imapuser.o sasl.o base64.o mailstorembox.o mailstore.o \
	deltaqueue.o deltaqueueaction.o deltaqueueidletimer.o deltaqueuedelayedmessage.o

include $(SOURCES:.cpp=.d)

imapd.o:  Makefile

imapsession.o:  Makefile

socket.o:  Makefile

imapserver.o:  Makefile

imapunixuser.o:  Makefile

imapuser.o:  Makefile

sasl.o:  Makefile

base64.o:  Makefile

mailstorembox.o:  Makefile

mailstore.o:  Makefile

deltaqueue.o:  Makefile

deltaqueueidletimer.o:  Makefile

deltaqueueaction.o:  Makefile

deltaqueuedelayedmessage.o: Makefile

clean:
	rm -f *.o *.d imapd

