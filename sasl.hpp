#if !defined(_SASL_HPP_INCLUDED_)
#define _SASL_HPP_INCLUDED_

#include "string"

#include "insensitive.hpp"

class ImapSession;

class Sasl {
public:
    typedef enum {
	ok, // user is logged in
	no, // user is not logged in due to bad password or invalid auth mechanism
	bad // authentication exchange cancelled
    } status;
    Sasl(ImapSession *mySession);
    status IsAuthenticated() { return currentStatus;}
    // CUser getUser() { return authorizationEntity;}
    virtual void SendChallenge(char *challenge_buff) = 0;
    virtual status ReceiveResponse(const std::string &csLine2) = 0;

protected:
    status currentStatus;
    std::string authorizationEntity;
    ImapSession *session;
};

class SaslAnonymous : public Sasl {
public:
    SaslAnonymous(ImapSession *mySession) : Sasl(mySession) {};
    void SendChallenge(char *challenge_buff);
    Sasl::status ReceiveResponse(const std::string &csLine2);
};

class SaslPlain : public Sasl {
public:
    SaslPlain(ImapSession *mySession) : Sasl(mySession) {};
    void SendChallenge(char *challenge_buff);
    Sasl::status ReceiveResponse(const std::string &csLine2);
};

// SYZYGY -- if the user data has access to the plaintext password, the rest of these can work
#if 0
class CSaslDigestMD5 : public CSasl {
public:
    CSaslDigestMD5(CIMapClient *myClient) : CSasl(myClient) {};
    CStdString SendChallenge();
    CSasl::status ReceiveResponse(const CStdString &csLine2);

private:
    CStdString m_csNonce;
};

class CSASLCramMD5 : public CSasl {
public:
    CSASLCramMD5(CIMapClient *myClient) : CSASL(myClient) {};
    CStdString SendChallenge();
    CSASL::status ReceiveResponse(CStdString &csLine2);

private:
    CStdString m_csChallenge;
};
#endif /* 0 */

typedef std::basic_string<char, caseInsensitiveTraits> insensitiveString;
Sasl *SaslChooser(ImapSession *mySession, const insensitiveString &residue);

#endif /* _SASL_HPP_INCLUDED_ */
