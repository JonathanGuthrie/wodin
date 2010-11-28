#if !defined(_SASL_HPP_INCLUDED_)
#define _SASL_HPP_INCLUDED_

#include <string>

#include <clotho/insensitive.hpp>

class ImapMaster;

class Sasl {
public:
  typedef enum {
    ok, // user is logged in
    no, // user is not logged in due to bad password or invalid auth mechanism
    bad // authentication exchange cancelled
  } status;
  Sasl(ImapMaster *myMaster);
  status isAuthenticated() { return m_currentStatus;}
  const std::string getUser() const { return m_authorizationEntity;}
  virtual void sendChallenge(char *challenge_buff) = 0;
  virtual status receiveResponse(const std::string &csLine2) = 0;

protected:
  status m_currentStatus;
  std::string m_authorizationEntity;
  ImapMaster *m_master;
};

class SaslAnonymous : public Sasl {
public:
  SaslAnonymous(ImapMaster *myMaster) : Sasl(myMaster) {};
  void sendChallenge(char *challenge_buff);
  Sasl::status receiveResponse(const std::string &csLine2);
};

class SaslPlain : public Sasl {
public:
  SaslPlain(ImapMaster *myMaster) : Sasl(myMaster) {};
  void sendChallenge(char *challenge_buff);
  Sasl::status receiveResponse(const std::string &csLine2);
};

// SYZYGY -- if the user data has access to the plaintext password, the rest of these can work
#if 0
class SaslDigestMD5 : public CSasl {
public:
  SaslDigestMD5(ImapMaster *master) : Sasl(master) {};
  void sendChallenge();
  Sasl::status receiveResponse(const CStdString &csLine2);

private:
  std::string m_nonce;
};

class SaslCramMD5 : public CSasl {
public:
  SaslCramMD5(ImapMaster *master) : Sasl(master) {};
  std::string sendChallenge();
  Sasl::status receiveResponse(CStdString &csLine2);

private:
  std::string m_challenge;
};
#endif /* 0 */

typedef std::basic_string<char, caseInsensitiveTraits> insensitiveString;
Sasl *SaslChooser(ImapMaster *master, const insensitiveString &residue);

#endif /* _SASL_HPP_INCLUDED_ */
