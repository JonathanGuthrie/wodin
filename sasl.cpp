#include <string>

#include <stdio.h>
#include <stdlib.h>

#include <clotho/internetserver.hpp>

#include "imapmaster.hpp"
#include "sasl.hpp"
#include "base64.hpp"
#include "imapuser.hpp"

// #include "md5.hpp" // SYZYGY -- only needed for CRAM-MD5 or DIGEST-MD5, which are commented out right now

std::string qstring(std::string &source) {
  std::string result;

  if ('"' == source[0]) {
    /* It looks like a quoted string */
    bool notdone = true;
    bool escape_flag = false;
    int pos = 1;
    do {
      // std::cout << "pos = " << pos << "\n";
      pos = (int)source.find_first_of("\"\\", pos);
      if (std::string::npos != pos) {
	if ('\\' == source[pos]) {
	  escape_flag = !escape_flag;
	}
	else {
	  if (escape_flag) {
	    // It's not the end of the string
	    escape_flag = false;
	  }
	  else {
	    // Copy the string to the destination 
	    result = source.substr(1, pos-1);
	    // and trim the source
	    source = source.substr(pos+1);
	    // and remove all the escape characters from the destination
	    pos = 0;
	    while(std::string::npos != (pos = result.find('\\', pos))) {
	      if (pos > 0) {
		result = result.substr(0, pos) + result.substr(pos+1);
	      }
	      else {
		result = result.substr(pos+1);
	      }
	      ++pos;
	    }
	    notdone = false;
	  }
	}
	++pos;
      }
      else {
	notdone = false;
      }
    } while (notdone);
  }
  return result;
}

inline std::string tohex(std::string s) {
  char buff[3];
  std::string result;

  int len = s.size();
  for(int i=0; i<len; ++i) { 
    sprintf(buff, "%02x", 0xff & s[i]);
    result += buff;
  }
  return result;
}

/*
 * The general approach taken for this is like this.  The residue should have the
 * authentication method and nothing else following it.  Or maybe it will have
 * some base-64 encoded data in it.  I don't know.  I need to join the mailing list.
 * Anyway, I will accept the "PLAIN", "CRAM-MD5" or "DIGEST-MD5" methods of authentication.
 * The general procedure is to send a challenge (a simple '+' for "PLAIN" or
 * something more complicated for the others.
 */
Sasl *SaslChooser(ImapMaster *master, const insensitiveString &residue) {
  Sasl *result;

  // std::cout << "The residue is \"" << residue << "\"\n";
  if (0 == residue.compare("PLAIN")) {
    result = new SaslPlain(master);
  }
  else {
#if 0
    if (0 == residue.compare("CRAM-MD5")) {
      result = new SaslCramMD5(master);
    }
    else {
      if (0 == residue.compare("DIGEST-MD5")) {
	result = new SaslDigestMD5(master);
      }
      else {
#endif /* 0 */
	if (master->IsAnonymousEnabled() && (0 == residue.compare("ANONYMOUS"))) {
	  result = new SaslAnonymous(master);
	}
	else {
	  result = NULL;
	}
#if 0
      }
    }
#endif /* 0 */
  }
  return result;
}

Sasl::Sasl(ImapMaster *master) {
  m_authorizationEntity = "";
  this->m_master = master;
}

void SaslAnonymous::SendChallenge(char *challenge_buff) {
  challenge_buff[0] = '\0';
}

Sasl::status SaslAnonymous::ReceiveResponse(const std::string &csLine2) {
  // RFC 2245 says that the response is optional, but that it can contain
  // tracking stuff.  I just ignore it until I have a use for it.
  m_authorizationEntity = "";
  m_currentStatus = ok;
  return m_currentStatus;
}

void SaslPlain::SendChallenge(char *challenge_buff) {
  challenge_buff[0] = '\0';
}

Sasl::status SaslPlain::ReceiveResponse(const std::string &csLine2) {
  // What it gets back from the read_command is a base-64 encoded string with
  // the authentication stuff in it.  It's base-64 encoded because the response
  // is supposed to include NUL characters in it.
  if ((csLine2.size() < 3) || (('*' == csLine2[0]) && ('\r' == csLine2[1]) && ('\n' == csLine2[2]))) {
    m_currentStatus = bad;
  }
  else  {
    std::string line = base64_decode(csLine2);
    // Here, 'line' contains a login which may has three parts separated
    // by NULL characters.  The parts are the authorization entity, the
    // authentication entity, and the password.  The authorization entity
    // is who is logging in, the authentication entity is who is vouching
    // for the authorization entity and the password is the password that
    // belongs to the authentication entity.  In the most common case, the
    // authentication agent and the authorization agent are the same and
    // the authorization field has zero length.
    if ('\0' == line[0]) {
      int pos = line.find('\0', 1);
      std::string author = line.substr(1, pos);
      std::string password = line.substr(pos+1);
      ImapUser *userData = m_master->userInfo(author.c_str());
      if (userData->CheckCredentials(password.c_str())) { // Note: Assuming "not secure"
	m_authorizationEntity = author;
	m_currentStatus = ok;
      }
      else {
	m_authorizationEntity = "";
	m_currentStatus = no;
      }
    }
    else {
      int pos = line.find('\0');
      std::string author = line.substr(0, pos);
      line = line.substr(pos+1);
      pos = line.find('\0');
      std::string authen = line.substr(0, pos);
      std::string password = line.substr(pos+1);
      ImapUser *userData = m_master->userInfo(author.c_str());
      if (userData->CheckCredentials(password.c_str())) { // Note: Assuming "not secure"
	m_authorizationEntity = author;
	m_currentStatus = ok;
      }
      else {
	m_authorizationEntity = "";
	m_currentStatus = no;
      }
    }
  }	
  return m_currentStatus;
}

#if 0
CStdString CSaslDigestMD5::SendChallenge() {
  m_eCurrentStatus = no;

  // DANGER!  WIL ROBINSON!  DANGER!
  long int r1= random(), r2 = random(), r3 = random();
  // DANGER!  WIL ROBINSON!  DANGER!

  CStdString nonce_value;
  m_csNonce += 0xff & (r1 >> 16);
  m_csNonce += 0xff & (r2 >> 16);
  m_csNonce += 0xff & (r3 >> 16);

  m_csNonce += 0xff & (r3 >> 8);
  m_csNonce += 0xff & (r1 >> 8);
  m_csNonce += 0xff & (r2 >> 8);

  m_csNonce += 0xff & (r2 >> 0);
  m_csNonce += 0xff & (r3 >> 0);
  m_csNonce += 0xff & (r1 >> 0);

  m_csNonce = base64_encode(m_csNonce);
  CStdString challenge;
  if (0 < m_cClient->GetAuthenticationRealm().size()) {
    challenge += _T("realm=\"") + m_cClient->GetAuthenticationRealm() + _T("\",");
  }
  challenge += _T("nonce=\"") + m_csNonce + _T("\",qop=\"auth\",algorithm=\"md5-sess\"");
  return base64_encode(challenge);
}

CSasl::status CSASLDigestMD5::ReceiveResponse(const CStdString &csLine2) {
  // std::cout << "The reply is \"" << reply << "\"\n";
  if ((csLine2.size() < 3) || (('*' == csLine2[0]) && ('\r' == csLine2[1]) && ('\n' == csLine2[2]))) {
    m_eCurrentStatus = bad;
  }
  else {
    CStdString reply = base64_decode(csLine2);
    CStdString username, realm, rcvdNonce, cnonce, nc, uri, response, authzid, temp, tname;
    CStdString parsed;
    CStdString qop(_T("auth"));
    parsed = reply;
    int usernameCount = 0, realmCount = 0, rcvdNonceCount = 0, cnonceCount = 0, ncCount = 0;
    int qopCount = 0, uriCount = 0, responseCount = 0, authzidCount = 0;

    while ('\0' != parsed[0]) {
      int pos = parsed.Find('=');
      if (CStdString::npos != pos) {
	tname = parsed.Mid(0, pos);
	parsed = parsed.Mid(pos+1);
	if (0 == tname.CompareNoCase("username")) {
	  ++usernameCount;
	  // username is a quoted string
	  username = qstring(parsed);
	}
	else {
	  if (0 == tname.CompareNoCase(_T("realm"))) {
	    ++realmCount;
	    // realm is a quoted string
	    realm = qstring(parsed);
	  }
	  else {
	    if (0 == tname.CompareNoCase(_T("nonce"))) {
	      ++rcvdNonceCount;
	      // nonce is a quoted string
	      rcvdNonce = qstring(parsed);
	    }
	    else {
	      if (0 == tname.CompareNoCase(_T("cnonce"))) {
		++cnonceCount;
		// cnonce is a quoted string
		cnonce = qstring(parsed);
	      }
	      else {
		if (0 == tname.CompareNoCase(_T("nc"))) {
		  ++ncCount;
		  // nc is NOT a quoted string
		  if (CStdString::npos != (pos = parsed.Find(','))) {
		    nc = parsed.Mid(0, pos);
		    parsed = parsed.Mid(pos);
		  }
		  else {
		    nc = parsed;
		    parsed = "";
		  }
		}
		else {
		  if (0 == tname.CompareNoCase(_T("qop"))) {
		    ++qopCount;
		    // qop is NOT quoted string
		    if (CStdString::npos != (pos = parsed.Find(','))) {
		      qop = parsed.Mid(0, pos);
		      parsed = parsed.Mid(pos);
		    }
		    else {
		      qop = parsed;
		      parsed = "";
		    }
		  }
		  else {
		    if (0 == tname.CompareNoCase(_T("digest-uri"))) {
		      ++uriCount;
		      // uri is a quoted string
		      uri = qstring(parsed);
		    }
		    else {
		      if (0 == tname.CompareNoCase(_T("response"))) {
			++responseCount;
			// response is NOT a quoted string
			if (CStdString::npos != (pos = parsed.Find(','))) {
			  response = parsed.Mid(0, pos);
			  parsed = parsed.Mid(pos);
			}
			else {
			  response = parsed;
			  parsed = _T("");
			}
		      }
		      else {
			if (0 == tname.CompareNoCase(_T("authzid"))) {
			  ++authzidCount;
			  // authzid is a quoted string
			  authzid = qstring(parsed);
			}
		      }
		    }
		  }
		}
	      }
	    }
	  }
	}
      }
      else {
	// std::cout << "Didn't find an equals sign\n";
	// std::cout << "Parsed = \"" << parsed << "\"\n";
	m_eCurrentStatus = no;
	return m_eCurrentStatus;
      }
      if (CStdString::npos != (pos = parsed.Find(','))) {
	parsed = parsed.Mid(pos+1);
      }
    }

    /*
     * At some point, I may also need to check that the that the uri is sane
     *
     * I don't check this now because I don't know how a sane value looks different from 
     * an insane value
     */
    if ((1 < realmCount) || (1 < qopCount) || (1 < uriCount) || (1 < authzidCount) || 
	(1 != usernameCount) || (1 != rcvdNonceCount) || (1 != cnonceCount) ||
	(1 != ncCount) || (1 != responseCount) || (0 != nc.Compare(_T("00000001"))) ||
	((1 == realmCount) && (0 != realm.Compare(m_cClient->GetAuthenticationRealm()))) ||
	((1 == qopCount) && (0 != qop.Compare(_T("auth")))) ||
	(0 != rcvdNonce.Compare(m_csNonce))) {
      // std::cout << "The reply was badly formatted\n";
      // std::cout << reply << "\n";
      // std::cout << "realmCount = " << realmCount << "\n";
      // std::cout << "qopCount = " << qopCount << "\n";
      // std::cout << "uriCount = " << uriCount << "\n";
      // std::cout << "authzidCount = " << authzidCount << "\n";
      // std::cout << "usernameCount = " << usernameCount << "\n";
      // std::cout << "rcvdNonceCount = " << rcvdNonceCount << "\n";
      // std::cout << "cnonceCount = " << cnonceCount << "\n";
      // std::cout << "ncCount = " << ncCount << "\n";
      // std::cout << "responseCount = " << responseCount << "\n";
      // std::cout << "nc = " << nc << "\n";
      // std::cout << "realm = " << realm << "\n";
      // std::cout << "qop = " << qop << "\n";
      // std::cout << "rcvdNonce = " << rcvdNonce << "\n";
      // std::cout << "nonce_value = " << nonce_value << "\n";
      m_eCurrentStatus = no;
      return m_eCurrentStatus;
    }
	
    CStdString passwd = lookupPassword(username);

    // std::cout << "passwd = " << passwd << "\n";

    // std::cout << "realm = " << realm << "\n";
    // std::cout << "qop = " << qop << "\n";
    // std::cout << "uri = " << uri << "\n";
    // std::cout << "authzid = " << authzid << "\n";
    // std::cout << "username = " << username << "\n";
    // std::cout << "rcvdNonce = " << rcvdNonce << "\n";
    // std::cout << "cnonce = " << cnonce << "\n";
    // std::cout << "nc = " << nc << "\n";
    // std::cout << "response = " << response << "\n";
    // std::cout << "nc = " << nc << "\n";
	
    MD5 hasher;

    CStdString A1 = username+_T(":")+realm+_T(":")+passwd;
    // std::cout << "A1 = " << A1 << "\n";
    hasher.update(A1);
    A1 = hasher.value()+_T(":")+rcvdNonce+_T(":")+cnonce;
    hasher.update(A1);
    A1 = tohex(hasher.value());
    CStdString A2 = ((CStdString)_T("AUTHENTICATE:"))+uri;
    // std::cout << "A2 = " << A2 << "\n";
    hasher.update(A2);
    A2 = tohex(hasher.value());
    CStdString checkVal = A1 + _T(":") + rcvdNonce + _T(":00000001:") + cnonce + _T(":auth:")+ A2;
    hasher.update(checkVal);
    checkVal = tohex(hasher.value());
    if (checkVal.Equals(response)) {
      m_eCurrentStatus = ok;
      A2 = ((CStdString)_T(":")) + uri;
      hasher.update(A2);
      A2 = tohex(hasher.value());
      response = A1 + _T(":") + rcvdNonce + _T(":00000001:") + cnonce + _T(":auth:")+ A2;
      hasher.update(response);
      response = tohex(hasher.value()); 
      reply = _T("rspauth=") + response;
      reply = ((CStdString) _T("+ ")) + base64_encode(response) + _T("\r\n");
      m_cClient->Send((void *)reply.data(), reply.size());
    }
  }
  return m_eCurrentStatus;
}

static bool hmac_check(CStdString &uid, CStdString &challenge, CStdString &credential) {
  MD5 hasher;

  CStdString pw = lookupPassword(uid);

  int len = pw.GetLength();
  if (64 < len) {
    // The actual secret is the md5 hash of the password
    hasher.update(pw);
    pw = hasher.value();
    len = 64;
  }
  CStdString k(pw);

  // It's less than or equal to 64 bytes, so I (may) have to pad it to 64 bytes and xor with ipad
  for (int i=0; i<64; ++i) {
    if (i < len) {
      k[i] ^= '\x36';
    }
    else {
      k += '\x36';
    }
  }
  k += challenge;
  // Retrieving the value of a hash resets the state of the hash so I can calculate another hash
  hasher.update(k);
  k = hasher.value();
  for (int i=0; i<64; ++i) {
    if (i < len) {
      pw[i] ^= '\x5c';
    }
    else {
      pw += '\x5c';
    }
  }
  pw += k;
  // Retrieving the value of a hash resets the state of the hash so I can calculate another hash
  hasher.update(pw);
  CStdString response = tohex(hasher.value());
  return response.Equals(credential);
}

CStdString CSaslCramMD5::SendChallenge() {
  // DANGER!  WIL ROBINSON!  DANGER!
  long int r1= random(), r2 = random();
  // DANGER!  WIL ROBINSON!  DANGER!
  char buff[15];
  CStdString h(m_cClient->GetHostname());

  sprintf(buff, _T("%u"), (0xffff & (r1 >> 16)) | (0xffff & (r2 >> 16)));
  m_csChallenge = ((CStdString)_T("<")) + buff + _T(".");
  sprintf(buff, _T("%u"), time(NULL));
  m_csChallenge += ((CStdString) buff) + _T("@") + h + _T(">");
  return base64_encode(m_csChallenge);
}

CSasl::status CSaslCramMD5::ReceiveResponse(CStdString &csLine2) {
  if ((csLine2.size() < 3) || (('*' == csLine2[0]) && ('\r' == csLine2[1]) && ('\n' == csLine2[2]))) {
    m_eCurrentStatus = bad;
  }
  else {
    // Okay, at this point, b should contain a string that is the base64 encoding of the 
    // response.  The response is the userid followed by a space followed by the lowercase
    // hexadecimal representation of an hmac-md5 message encrypted with the secret key that
    //  is the user's password.  Therefore, the first step is to un-base64 it.
    CStdString response = base64_decode(csLine2);

    int pos = response.Find(' ');
    CStdString author = response.Mid(0, pos);
    CStdString password = response.Mid(pos+1);
    // std::cout << "The author is \"" << author << "\"\n";
    // std::cout << "The password is \"" << password << "\"\n";
    if (hmac_check(author, m_csChallenge, password)) {
      m_csAuthorizationEntity = author;
      m_eCurrentStatus = ok;
    }
    else {
      m_csAuthorizationEntity = _T("");
      m_eCurrentStatus = no;
    }
  }
  return m_eCurrentStatus;
}
#endif /* 0 */

#ifdef SASL_DEBUG
#include <iostream>

int
main() {
  CStdString challenge("<1896.697170952@postoffice.reston.mci.net>");
  CStdString password("tanstaaftanstaaf");
  CStdString credential("b913a602c7eda7a495b4e6e7334d3890");
  CStdString uid("tim");

  if (hmac_check(uid, challenge, credential)) {
    std::cout << "The check succeeded\n";
  }
  else {
    std::cout << "The check failed\n";
  }
}
#endif /* DEBUG */
