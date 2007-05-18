CC=g++
CXXFLAGS=-g
LDFLAGS=-lpthread -lcrypt

imapd: imapd.o imapserver.o imapsession.o socket.o imapunixuser.o imapuser.o sasl.o base64.o mailstorembox.o mailstore.o

imapd.o:  imapd.cpp imapserver.hpp socket.hpp ThreadPool.hpp Makefile

imapsession.o:  imapsession.cpp imapsession.hpp socket.hpp mailstore.hpp sasl.hpp Makefile

socket.o:  socket.cpp socket.hpp Makefile

imapserver.o:  imapserver.cpp imapserver.hpp socket.hpp imapsession.hpp ThreadPool.hpp imapunixuser.hpp Makefile

imapunixuser.o:  imapunixuser.cpp imapunixuser.hpp imapuser.hpp Makefile

imapuser.o:  imapuser.cpp imapuser.hpp Makefile

sasl.o:  sasl.cpp sasl.hpp imapsession.hpp imapserver.hpp base64.hpp Makefile

base64.o: base64.cpp base64.hpp Makefile

mailstorembox.o:  mailstorembox.cpp mailstorembox.hpp mailstore.hpp imapsession.hpp imapserver.hpp Makefile

mailstore.o:  mailstore.cpp mailstore.hpp Makefile

clean:
	rm -f *.o imapd
