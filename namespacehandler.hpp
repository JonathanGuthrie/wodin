#if !defined(_NAMESPACEHANDLER_HPP_INCLUDED)
#define _NAMESPACEHANDLER_HPP_INCLUDED

#include "imaphandler.hpp"

ImapHandler *namespaceHandler(ImapSession *, INPUT_DATA_STRUCT &input);

class NamespaceHandler : public ImapHandler {
public:
  NamespaceHandler(ImapSession *session)  : ImapHandler(session) {}
  virtual ~NamespaceHandler() {}
  virtual IMAP_RESULTS receiveData(INPUT_DATA_STRUCT &input);
};

#endif // !defined(_NAMESPACEHANDLER_HPP_INCLUDED)
