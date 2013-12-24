GTEST_DIR=/usr/src/gtest
GMOCK_DIR=/usr/src/gmock

CPPFLAGS += -isystem $(GTEST_DIR)/include -I..
CC=g++
CXXFLAGS=-g -Wall -Wextra -pthread
CXXTESTFLAGS=$(CXXFLAGS) -DTEST
LDFLAGS=-lpthread -lcrypt -lclotho -lboost_system gtest_main.a

SOURCES = 	../imapsession.cpp \
	../imapunixuser.cpp \
	../imapuser.cpp \
	../sasl.cpp \
	../base64.cpp \
	../mailstore.cpp \
	../mailstorembox.cpp \
	../idletimer.cpp \
	../asynchronousaction.cpp \
	../delayedmessage.cpp \
	../retry.cpp \
	../namespace.cpp \
	../mailstoreinvalid.cpp \
	../datetime.cpp \
	../mailsearch.cpp \
	../mailmessage.cpp \
	../imapmaster.cpp \
	../unimplementedhandler.cpp \
	../capabilityhandler.cpp \
	../noophandler.cpp \
	../logouthandler.cpp \
	../authenticatehandler.cpp \
	../loginhandler.cpp \
	../namespacehandler.cpp \
	../createhandler.cpp \
	../deletehandler.cpp \
	../renamehandler.cpp \
	../subscribehandler.cpp \
	../listhandler.cpp \
	../statushandler.cpp \
	../appendhandler.cpp \
	../selecthandler.cpp \
	../checkhandler.cpp \
	../expungehandler.cpp \
	../copyhandler.cpp \
	../closehandler.cpp \
	../searchhandler.cpp \
	../fetchhandler.cpp \
	../storehandler.cpp

TESTS = misc_command_handlers \
	append_handler

TEST_SOURCES = misc_command_handlers.cc \
	append_handler.cc

# All Google Test headers.  Usually you shouldn't change this
# definition.
GTEST_HEADERS = /usr/include/gtest/*.h \
                /usr/include/gtest/internal/*.h

# Usually you shouldn't tweak such internal variables, indicated by a
# trailing _.
GTEST_SRCS_ = $(GTEST_DIR)/src/*.cc $(GTEST_DIR)/src/*.h $(GTEST_HEADERS)
GMOCK_SRCS_ = $(GMOCK_DIR)/src/*.cc

all: gtest.a gtest_main.a $(TESTS)

.PHONY: clean
clean:
	rm -f $(TESTS) gtest.a gtest_main.a *.o *.d

# For simplicity and to avoid depending on Google Test's
# implementation details, the dependencies specified below are
# conservative and not optimized.  This is fine as Google Test
# compiles fast and for ordinary users its source rarely changes.

# Builds gtest.a and gtest_main.a.

gtest-all.o : $(GTEST_SRCS_)
	$(CXX) $(CPPFLAGS) -I$(GTEST_DIR) $(CXXFLAGS) -c $(GTEST_DIR)/src/gtest-all.cc

gmock-all.o : $(GMOCK_SRCS_)
	$(CXX) $(CPPFLAGS) -I$(GMOCK_DIR) $(CXXFLAGS) -c $(GMOCK_DIR)/src/gmock-all.cc

gtest_main.o : $(GTEST_SRCS_)
	$(CXX) $(CPPFLAGS) -I$(GTEST_DIR) $(CXXFLAGS) -c $(GTEST_DIR)/src/gtest_main.cc

gtest.a : gtest-all.o gmock-all.o
	$(AR) $(ARFLAGS) $@ $^

gtest_main.a : gtest-all.o gmock-all.o gtest_main.o
	$(AR) $(ARFLAGS) $@ $^

%.d: ../%.cpp
	@set -e; rm -f $@; \
	$(CC) -MM $(CPPFLAGS) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : Makefile $(GTEST_HEADERS) ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

%.d: %.cc
	@set -e; rm -f $@; \
	$(CC) -MM $(CPPFLAGS) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : Makefile $(GTEST_HEADERS) ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

%.o: ../%.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $<

misc_command_handlers: misc_command_handlers.o $(SOURCES:../%.cpp=%.o) gtest_main.a

append_handler: append_handler.o $(SOURCES:../%.cpp=%.o) gtest_main.a

include $(TEST_SOURCES:.cc=.d)

include $(SOURCES:../%.cpp=%.d)

