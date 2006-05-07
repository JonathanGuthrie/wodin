#include <iostream>
#include <vector>
#include <fstream>

#include <dirent.h>
#include <unistd.h>
#include <regex.h>
#include <sys/stat.h>

#include "mailstorembox.hpp"

#define MAILBOX_LIST_FILE_NAME ".mailboxlist"

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
bool MailStoreMbox::ListAllHelper(const regex_t *compiled_regex, const char *home_directory, const char *working_dir, MAILBOX_LIST *result, int maxdepth)
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
	    if ((('.' != entry->d_name[0]) || ('\0' != entry->d_name[1])) &&
		(('.' != entry->d_name[0]) || ('.' != entry->d_name[1]) || ('\0' != entry->d_name[2])))
	    {
		returnValue = true;
		struct stat stat_buf;
		char short_path[PATH_MAX];

		sprintf(short_path, "%s/%s", working_dir, entry->d_name);
		sprintf(full_path, "%s/%s", home_directory, short_path);
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
			    // I don't go down any more if the depth is zero
			    if (maxdepth != 0)
			    {
				if (ListAllHelper(compiled_regex, home_directory, short_path, result, (maxdepth > 0) ? maxdepth-1 : maxdepth))
				{
				    name.attributes |= MailStore::IMAP_MBOX_HASCHILDREN;
				}
			    }
			}
		    }
		    else
		    {
			name.attributes = MailStore::IMAP_MBOX_NOINFERIORS;
			if (isMailboxInteresting(name.name))
			{
			    name.attributes |= MailStore::IMAP_MBOX_MARKED;
			}
			else
			{
			    name.attributes |= MailStore::IMAP_MBOX_UNMARKED;
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
    return false;  // SYZYGY -- need to find out how c-client defines "interesting"
}

bool MailStoreMbox::isMailboxInteresting(std::string mailbox)
{
    return false; // SYZYGY -- need to find out how c-client defines "interesting"
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
	    name.attributes = MailStore::IMAP_MBOX_NOINFERIORS;
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

    // So, I've special-cased "inbox".  For the rest, I was originally going to
    // recurse down the directory tree starting in the user's home directory and
    // adding each result that matches the pattern.

    // The problem is that the performance of this sucks.  There are two ways to
    // improve the performance of list that immediately come to mind.  First, don't
    // start at the home directory, but start somewhere underneath it, if possible.
    // Second, don't recurse down unless the pattern allows it.

    // To do the first, I skip over the non-wildcard characters and then back up to
    // to the preceeding separator character and that's where I start recursing.

    // To do the second, well, I can't do it if there are any stars in the pattern,
    // but I can count the number of separators in the wildcarded section and only
    // recurse down that many.

    // The matching is done by converting
    // the pattern into a regular expression and then seeing if that regular
    // expression matches the strings

    // recursing down the directory tree depending on the pattern in "pattern".
    // The first thing to do is to find the end of the span of characters that
    // does not include wildcards.
    if (0 == regcomp(&compiled_regex, regex, REG_EXTENDED | REG_NOSUB))
    {
	char base_path[PATH_MAX];
	size_t static_len;
	// maxdepth starts at 1 so I can set HASCHILDREN properly
	int maxdepth = 1;

	static_len = strcspn(pattern, "%*");
	for (int i=static_len; pattern[i] != '\0'; ++i)
	{
	    if ('*' == pattern[i])
	    {
		// maxdepth of -1 implies no limit
		maxdepth = -1;
		break;
	    }
	    if ('/' == pattern[i])
	    {
		++maxdepth;
	    }
	}
	for (; static_len>0; --static_len)
	{
	    if ('/' == pattern[static_len])
	    {
		break;
	    }
	}
	sprintf(base_path, "%s/%.*s", homeDirectory, static_len, pattern);
	DIR *directory = opendir(base_path);
	if (NULL != directory)
	{
	    struct dirent *entry;
	    while (NULL != (entry = readdir(directory)))
	    {
		if ((('.' != entry->d_name[0]) || ('\0' != entry->d_name[1])) &&
		    (('.' != entry->d_name[0]) || ('.' != entry->d_name[1]) || ('\0' != entry->d_name[2])))
		{
		    char short_path[PATH_MAX];
		    char full_path[PATH_MAX];

		    if (static_len > 0)
		    {
			sprintf(short_path, "%.*s/%s", static_len, pattern, entry->d_name);
		    }
		    else
		    {
			strcpy(short_path, entry->d_name);
		    }
		    sprintf(full_path, "%s/%s", homeDirectory, short_path);
		    struct stat stat_buf;
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
				if (ListAllHelper(&compiled_regex, homeDirectory, short_path, result, maxdepth))
				{
				    name.attributes |= MailStore::IMAP_MBOX_HASCHILDREN;
				}
			    }
			}
			else
			{
			    name.attributes = MailStore::IMAP_MBOX_NOINFERIORS;
			    if (isMailboxInteresting(name.name))
			    {
				name.attributes |= MailStore::IMAP_MBOX_MARKED;
			    }
			    else
			    {
				name.attributes |= MailStore::IMAP_MBOX_UNMARKED;
			    }
			}
			if (0 == regexec(&compiled_regex, short_path, 0, NULL, 0))
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



void MailStoreMbox::ListSubscribed(const char *pattern, MAILBOX_LIST *result)
{
    bool inbox_matches = false;
    // In the mbox world, at least on those systems handled by c-client, it appears
    // as if the list of folders subscribed to is stored in a file, one per line,
    // so to do this command, I just read that file and match against pattern.

    // First, convert the pattern to a regex and compile the regex
    regex_t compiled_regex;
    char *regex = new char[3+5*strlen(pattern)];

    ConvertPatternToRegex(pattern, regex);

    // The regex must be compiled twice.  For matching "inbox", I enable ignoring 
    // case.  For the regular matches, I'll use case-specific matching.
    if (0 == regcomp(&compiled_regex, regex, REG_EXTENDED | REG_ICASE | REG_NOSUB))
    {
	if (0 == regexec(&compiled_regex, "inbox", 0, NULL, 0))
	{
	    // If I get here, then I know that inbox matches the pattern.
	    // However, I don't know if I'm subscribed to inbox.  I'll have to
	    // look for that as part of the processing of the subscription
	    // file
	    inbox_matches = true;
	}
	regfree(&compiled_regex);
    }

    std::string file_name = homeDirectory;
    file_name += "/" MAILBOX_LIST_FILE_NAME;
    std::ifstream inFile(file_name.c_str());
    if (0 == regcomp(&compiled_regex, regex, REG_EXTENDED | REG_NOSUB))
    {
	while (!inFile.eof())
	{
	    std::string line;
	    inFile >> line;
	    // I have to check this here, because it's only set when attempting
	    // to read past the end of the file
	    if (!inFile.eof())
	    {
		const char *cstr_line;

		cstr_line = line.c_str();
		if (inbox_matches &&
		    (('i' == cstr_line[0]) || ('I' == cstr_line[0])) &&
		    (('n' == cstr_line[1]) || ('N' == cstr_line[1])) &&
		    (('b' == cstr_line[2]) || ('B' == cstr_line[2])) &&
		    (('o' == cstr_line[3]) || ('O' == cstr_line[3])) &&
		    (('x' == cstr_line[4]) || ('X' == cstr_line[4])) &&
		    ('\0' == cstr_line[5]))
		{
		    MAILBOX_NAME name;

		    name.name = "INBOX";
		    name.attributes = MailStore::IMAP_MBOX_NOINFERIORS;
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
		else
		{
		    if (0 == regexec(&compiled_regex, cstr_line, 0, NULL, 0))
		    {
			MAILBOX_NAME name;

			name.name = line;
			name.attributes = 0;
			// SYZYGY WORKING HERE
			// SYZYGY -- I need to determine whether or not the line names
			// SYZYGY -- a folder or a mailbox and handle accordingly
			// SYZYGY -- to check for children so I can set the flags
			if (isMailboxInteresting(name.name))
			{
			    name.attributes |= MailStore::IMAP_MBOX_MARKED;
			}
			else
			{
			    name.attributes |= MailStore::IMAP_MBOX_UNMARKED;
			}
			result->push_back(name);
		    }
		}
	    }
	}
	regfree(&compiled_regex);
    }
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
