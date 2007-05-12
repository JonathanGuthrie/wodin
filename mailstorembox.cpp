#include <iostream>
#include <vector>
#include <fstream>

#include <dirent.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/stat.h>

#include "mailstorembox.hpp"

#define MAILBOX_LIST_FILE_NAME ".mailboxlist"

MailStoreMbox::MailStoreMbox(const char *usersInboxPath, const char *usersHomeDirectory) : MailStore()
{
    inboxPath = strdup(usersInboxPath);
    homeDirectory = strdup(usersHomeDirectory);
}

// The CreateMailbox method deals with two cases.  Either the mailbox name is "inbox" which
// is considered special, or the mailbox name is a path relative to the user's home directory.
// If the mail box name ends in a slash, which is what I'm using as a "path separator", then
// I create a mail directory, otherwise I create a mail file.
MailStore::MAIL_STORE_RESULT MailStoreMbox::CreateMailbox(const std::string &MailboxName)
{
    if ((('i' == MailboxName[0]) || ('I' == MailboxName[0])) &&
	(('n' == MailboxName[1]) || ('N' == MailboxName[1])) &&
	(('b' == MailboxName[2]) || ('B' == MailboxName[2])) &&
	(('o' == MailboxName[3]) || ('O' == MailboxName[3])) &&
	(('x' == MailboxName[4]) || ('X' == MailboxName[4])) &&
	('\0' == MailboxName[5])) {
    }
    else {
	// SYZYGY -- working here!
    }

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

// c-client checks the ctime against the atime of the file in question and returns
// true if access time is after create or modify, else false
bool MailStoreMbox::isMailboxInteresting(const std::string path)
{
    int result = false;
    struct stat stat_buf;

    if (0 == lstat(path.c_str(), &stat_buf))
    {
	result = (stat_buf.st_atime < stat_buf.st_ctime) || (stat_buf.st_atime < stat_buf.st_mtime);
    }
    return result;
}

// If pattern is n characters long, then the "regex" destination buffer must be
// 5n+3 characters long to be assured of being long enough.
static void ConvertPatternToRegex(const char *pattern, char *regex, char isForLsub = false) {
    char lastchar;

    *regex++ = '^';
    if (isForLsub) {
	*regex++ = '(';
    }
    while('\0' != *pattern) {
	lastchar = *pattern;
	switch(*pattern) {
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
    if (isForLsub) {
	*regex++ = ')';
	if ('%' != lastchar) {
	    *regex++ = '$';
	}
    }
    else {
	*regex++ = '$';
    }
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
	    if (isMailboxInteresting(inboxPath))
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
    // 3+5*strlen(pattern) works for list, but I'm going to be putting the 
    // pattern in parentheses so I need 5+5*strlen for lsub
    char *regex = new char[5+5*strlen(pattern)];

    ConvertPatternToRegex(pattern, regex, true);

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
    if (0 == regcomp(&compiled_regex, regex, REG_EXTENDED))
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
		    if (isMailboxInteresting(inboxPath))
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
		    regmatch_t match;

		    if (0 == regexec(&compiled_regex, cstr_line, 1, &match, 0))
		    {
			MAILBOX_NAME name;

			// With list, I need to set the flags correctly, with LSUB, I don't.
			// So says RFC 3501 section 6.3.9
			// I do have to handle the weirdness associated with the trailing % flag, though
			// which is what all the "match" stuff is about
			if (match.rm_so != -1) {
			    name.name = line;
			    name.name.erase(match.rm_eo);
			    name.attributes = 0;
			    if (name.name != line) {
				name.attributes |= IMAP_MBOX_NOSELECT;
				name.attributes |= IMAP_MBOX_HASCHILDREN;
			    }
			    result->push_back(name);
			}
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



MailStore::MAIL_STORE_RESULT MailStoreMbox::SubscribeMailbox(const std::string &MailboxName, bool isSubscribe)
{
    MailStore::MAIL_STORE_RESULT result = MailStore::SUCCESS;
    bool foundLine = false;
    std::string in_file_name = homeDirectory;
    in_file_name += "/" MAILBOX_LIST_FILE_NAME;
    std::string out_file_name = homeDirectory;
    out_file_name += "/" MAILBOX_LIST_FILE_NAME;
    out_file_name += ".new";
    std::ifstream inFile(in_file_name.c_str());
    std::ofstream outFile(out_file_name.c_str());
    while (!inFile.eof())
    {
	std::string line;
	inFile >> line;
	// I have to check this here, because it's only set when attempting
	// to read past the end of the file
	if (!inFile.eof())
	{
	    if (MailboxName == line) {
		foundLine = true;
		if (isSubscribe) {
		    outFile << line << std::endl;
		}
	    }
	    else {
		outFile << line << std::endl;
	    }
	}
    }
    if (isSubscribe) {
	if (foundLine) {
	    result = MailStore::MAILBOX_ALREADY_SUBSCRIBED;
	}
	else {
	    outFile << MailboxName << std::endl;
	}
    }
    else {
	if (!foundLine) {
	    result = MailStore::MAILBOX_NOT_SUBSCRIBED;
	}
    }

    ::rename(out_file_name.c_str(), in_file_name.c_str());
    return result;
}


MailStoreMbox::~MailStoreMbox()
{
}
