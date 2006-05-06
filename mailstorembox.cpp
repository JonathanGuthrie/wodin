#include <iostream>
#include <vector>

#include <dirent.h>
#include <unistd.h>
#include <regex.h>
#include <sys/stat.h>

#include "mailstorembox.hpp"

MailStoreMbox::MailStoreMbox(const char *usersHomeDirectory) : MailStore()
{
    homeDirectory = strdup(usersHomeDirectory);
}

MailStore::MAIL_STORE_RESULT MailStoreMbox::CreateMailbox(const std::string &MailboxName)
{
    return MailStore::SUCCESS;
}

MailStore::MAIL_STORE_RESULT MailStoreMbox::MailboxClose()
{
    return MailStore::SUCCESS;
}

unsigned MailStoreMbox::GetSerialNumber()
{
}

unsigned MailStoreMbox::GetNextSerialNumber()
{
}

unsigned MailStoreMbox::MailboxMessageCount(const std::string &MailboxName)
{
}

unsigned MailStoreMbox::MailboxRecentCount(const std::string &MailboxName)
{
}

unsigned MailStoreMbox::MailboxFirstUnseen(const std::string &MailboxName)
{
}

std::string MailStoreMbox::GetMailboxUserPath() const
{
}

MailStore::MAIL_STORE_RESULT MailStoreMbox::MailboxUpdateStats(NUMBER_LIST *nowGone)
{
    return MailStore::SUCCESS;
}


// This function is different from ListAll in three ways.  First, it doesn't know that "inbox" is
// magic.  Second, it doesn't refer to MailStoreMbox's private data, third, it needs a precompiled
// regex instead of a pattern
static bool ListAllHelper(const regex_t *compiled_regex, const char *home_directory, const char *working_dir, MAILBOX_LIST *result)
{
    bool returnValue = false;
    char full_path[PATH_MAX];

    sprintf(full_path, "%s/%s", home_directory, working_dir);
    DIR *directory = opendir(full_path);
    if (NULL != directory)
    {
	struct dirent *entry;
	while (NULL != (entry = readdir(directory)))
	{
	    if (!(('.' == entry->d_name[0]) && ('\0' == entry->d_name[1])) &&
		!(('.' == entry->d_name[0]) && ('.' == entry->d_name[1]) && ('\0' == entry->d_name[2])))
	    {
		returnValue = true;
		struct stat stat_buf;
		char short_path[PATH_MAX];

		sprintf(short_path, "%s/%s", working_dir, entry->d_name);
		sprintf(full_path, "%s/%s", home_directory, short_path);
		// SYZYGY -- I need to set the value of the "mark" flag
		if (0 == lstat(full_path, &stat_buf))
		{
		    MAILBOX_NAME name;

		    name.name = short_path;
		    name.attributes = 0;
		    if (!S_ISREG(stat_buf.st_mode))
		    {
			name.attributes = MailStore::IMAP_MBOX_NOSELECT;
			if (S_ISDIR(stat_buf.st_mode))
			{
			    if (ListAllHelper(compiled_regex, home_directory, short_path, result))
			    {
				name.attributes |= MailStore::IMAP_MBOX_HASCHILDREN;
			    }
			}
		    }
		    if (0 == regexec(compiled_regex, short_path, 0, NULL, 0))
		    {
			result->push_back(name);
		    }
		}
	    }
	}
	closedir(directory);
    }
    return returnValue;
}

bool MailStoreMbox::isInboxInteresting(void)
{
    return false;  // SYZYGY
}

// If pattern is n characters long, then the "regex" destination buffer must be
// 5n+3 characters long to be assured of being long enough.
static void ConvertPatternToRegex(const char *pattern, char *regex)
{
    *regex++ = '^';
    while('\0' != *pattern)
    {
	switch(*pattern)
	{
	case '^':
	case ',':
	case '.':
	case '[':
	case '$':
	case '(':
	case ')':
	case '|':
	case '+':
	case '?':
	case '{':
	case '\\':
	    *regex++ = '\\';
	    *regex++ = *pattern;
	    break;

	case '*':
	    *regex++ = '.';
	    *regex++ = '*';
	    break;

	case '%':
	    *regex++ = '[';
	    *regex++ = '^';
	    *regex++ = '/';
	    *regex++ = ']';
	    *regex++ = '*';
	    break;

	default:
	    *regex++ = *pattern;
	    break;
	}
	++pattern;
    }
    *regex++ = '$';
    *regex = '\0';
}


void MailStoreMbox::ListAll(const char *pattern, MAILBOX_LIST *result)
{
    struct stat buff;
    regex_t compiled_regex;
    char *regex = new char[3+5*strlen(pattern)];

    ConvertPatternToRegex(pattern, regex);

    // For the mbox mail store, a list is usually the directory listing 
    // of some place on the computer.  It's usually under the home directory
    // of the user.  The exception is "INBOX" (case insensitive) which is
    // special-cased.

    // I'm going to do the matching using the available regular expression library.
    // To do that, I need to convert the pattern to a regular expression.
    // To convert the pattern to a regular expression, I need to change characters
    // in the pattern into regex equivalents.  "*" becomes ".*", "%" becomes
    // "[^/]*", and the characters "^", ".", "[", "$", "(", ")", "|", "+", "?", "{"
    // and "\" must be escaped by prepending a backslash.

    // The regex must be compiled twice.  For matching "inbox", I enable ignoring 
    // case.  For the regular matches, I'll use case-specific matching.
    if (0 == regcomp(&compiled_regex, regex, REG_EXTENDED | REG_ICASE | REG_NOSUB))
    {
	if (0 == regexec(&compiled_regex, "inbox", 0, NULL, 0))
	{
	    MAILBOX_NAME name;

	    name.name = "INBOX";
	    name.attributes = 0;
	    if (isInboxInteresting())
	    {
		name.attributes |= MailStore::IMAP_MBOX_MARKED;
	    }
	    else
	    {
		name.attributes |= MailStore::IMAP_MBOX_UNMARKED;
	    }
	    result->push_back(name);
	}
	regfree(&compiled_regex);
    }

    // So, I've special-cased "inbox".  For the rest, I'm going to recurse down
    // the directory tree starting in the user's home directory and I'll add
    // each result that matches the pattern.  I'll do the matching by converting
    // the pattern into a regular expression and then seeing if that regular
    // expression matches the strings

    // recursing down the directory tree depending on the pattern in "pattern".
    // The first thing to do is to find the end of the span of characters that
    // does not include wildcards.
    if (0 == regcomp(&compiled_regex, regex, REG_EXTENDED | REG_NOSUB))
    {
	DIR *directory = opendir(homeDirectory);
	if (NULL != directory)
	{
	    struct dirent *entry;
	    while (NULL != (entry = readdir(directory)))
	    {
		if (!(('.' == entry->d_name[0]) && ('\0' == entry->d_name[1])) &&
		    !(('.' == entry->d_name[0]) && ('.' == entry->d_name[1]) && ('\0' == entry->d_name[2])))
		{
		    struct stat stat_buf;
		    char buffer[PATH_MAX];
		    sprintf(buffer, "%s/%s", homeDirectory, entry->d_name);
		    // SYZYGY -- I need to set the value of the "mark" flag
		    if (0 == lstat(buffer, &stat_buf))
		    {
			MAILBOX_NAME name;

			name.name = entry->d_name;
			name.attributes = 0;
			if (!S_ISREG(stat_buf.st_mode))
			{
			    name.attributes = MailStore::IMAP_MBOX_NOSELECT;
			    if (S_ISDIR(stat_buf.st_mode))
			    {
				if (ListAllHelper(&compiled_regex, homeDirectory, entry->d_name, result))
				{
				    name.attributes |= MailStore::IMAP_MBOX_HASCHILDREN;
				}
			    }
			}
			if (0 == regexec(&compiled_regex, entry->d_name, 0, NULL, 0))
			{
			    result->push_back(name);
			}
		    }
		}
	    }
	    closedir(directory);
	}
	regfree(&compiled_regex);
    }
    delete[] regex;
}



static void ListSubscribed(const char *pattern, MAILBOX_LIST *result)
{
}



void MailStoreMbox::BuildMailboxList(const char *ref, const char *pattern, MAILBOX_LIST *result, bool listAll)
{
    char *ref2, *pat2;

    ref2 = strdup(ref);
    pat2 = strdup(pattern);
    if (listAll)
    {
	ListAll(pattern, result);
    }
    else
    {
	ListSubscribed(pattern, result);
    }
}



MailStoreMbox::~MailStoreMbox()
{
}
