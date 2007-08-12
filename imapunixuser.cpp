#include <iostream>

#include <pwd.h>
#include <shadow.h>
#include <ctype.h>

#include "imapunixuser.hpp"

ImapUnixUser::ImapUnixUser(const char *user) : ImapUser(user)
{
    home = NULL;
    struct passwd *pass;
    setpwent();
    while (NULL != (pass = getpwent()))
    {
	if (0 == strcmp(pass->pw_name, user))
	{
	    uid = pass->pw_uid;
	    gid = pass->pw_gid;
	    home = new char[1+strlen(pass->pw_dir)];
	    strcpy(home, pass->pw_dir);
	    endpwent();
	    userFound = true;
	    return;
	}
    }
    endpwent();
    // SYZYGY -- I think this should throw an exception for user not found
}

ImapUnixUser::~ImapUnixUser()
{
    if (NULL != home) {
	delete[] home;
    }
}

bool ImapUnixUser::HavePlaintextPassword()
{
    return false;
}

bool ImapUnixUser::CheckCredentials(const char *password)
{
    if (userFound)
    {
	struct spwd *shadow =  getspnam(name);
	bool result = false;

	if (NULL != shadow)
	{
	    // std::cout << "The encrypted password is \"" << shadow->sp_pwdp << "\"" << std::endl;
	    if (isalnum(shadow->sp_pwdp[0]) || ('$' == shadow->sp_pwdp[0]) ||
		('/' == shadow->sp_pwdp[0]) || ('\\' == shadow->sp_pwdp[0]) ||
		('.' == shadow->sp_pwdp[0]))
	    {
		char salt[32];
		if ('$' == shadow->sp_pwdp[0])
		{
		    char *end;

		    strncpy(salt, shadow->sp_pwdp, 31);
		    salt[31] = '\0';
		    end=strchr(salt+1, '$');
		    if (NULL != end)
		    {
			end = strchr(end+1, '$');
			if (NULL != end)
			{
			    *end = '\0';
			}
			else
			{
			    salt[0] = '\0';
			}
		    }
		    else
		    {
			salt[0] = '\0';
		    }
		}
		else
		{
		    strncpy(salt, shadow->sp_pwdp, 2);
		    salt[2] = '\0';
		}

		if ('\0' != salt[0])
		{
		    char *crypt_result = crypt(password, salt);
		    // std::cout << "The calculated password is \"" << crypt_result << "\"" << std::endl;
		    result = (0 == strcmp(crypt_result, shadow->sp_pwdp));
		}
	    }
	}
	return result;
    }
    else
    {
	return false;
    }
}


// I have no plaintext password, so I can't return it.
char *ImapUnixUser::GetPassword(void) const
{
    return NULL;
}
