/*
 * Copyright 2013 Jonathan R. Guthrie
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#if !defined(_SASL_HPP_INCLUDED_)
#define _SASL_HPP_INCLUDED_

#include <string>

#include <clotho/insensitive.hpp>

class ImapMaster;

// SASL is defined in RFC 4422
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

// ANONYMOUS is defined in RFC 4505
class SaslAnonymous : public Sasl {
public:
  SaslAnonymous(ImapMaster *myMaster) : Sasl(myMaster) {};
  void sendChallenge(char *challenge_buff);
  Sasl::status receiveResponse(const std::string &csLine2);
};

// PLAIN is defined in RFC 4616
class SaslPlain : public Sasl {
public:
  SaslPlain(ImapMaster *myMaster) : Sasl(myMaster) {};
  void sendChallenge(char *challenge_buff);
  Sasl::status receiveResponse(const std::string &csLine2);
};

// SYZYGY -- if the user data has access to the plaintext password, the rest of these can work
#if 0
// DIGEST-MD5 is defined in RFC 2831
class SaslDigestMD5 : public CSasl {
public:
  SaslDigestMD5(ImapMaster *master) : Sasl(master) {};
  void sendChallenge();
  Sasl::status receiveResponse(const CStdString &csLine2);

private:
  std::string m_nonce;
};

// CRAM-MD5 is defined in RFC 2195
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
