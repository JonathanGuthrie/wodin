#if !defined(_INSENSITIVE_HPP_INCLUDED_)
#define _INSENSITIVE_HPP_INCLUDED_

struct caseInsensitiveTraits : public std::char_traits<char> {
    // return whether c1 and c2 are equal
    static bool eq(const char& c1, const char& c2) {
	return std::toupper(c1)==std::toupper(c2);
    }
    static bool lt(const char& c1, const char& c2) {
        return std::toupper(c1)< std::toupper(c2);
    }
    // compare up to n characters of s1 and s2
    static int compare(const char* s1, const char* s2, std::size_t n) {
        for (std::size_t i=0; i<n; ++i)
	{
            if (!eq(s1[i],s2[i]))
	    {
                return lt(s1[i],s2[i])?-1:1;
            }
        }
        return 0;
    }
};


typedef std::basic_string<char, caseInsensitiveTraits> insensitiveString;

#endif // _INSENSITIVE_HPP_INCLUDED_
