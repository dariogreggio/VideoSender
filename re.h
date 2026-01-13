/*
 *
 * Mini regex-module inspired by Rob Pike's regex code described in:
 *
 * http://www.cs.princeton.edu/courses/archive/spr09/cos333/beautiful.html
 *
 *
 *
 * Supports:
 * ---------
 *   '.'        Dot, matches any character
 *   '^'        Start anchor, matches beginning of string
 *   '$'        End anchor, matches end of string
 *   '*'        Asterisk, match zero or more (greedy)
 *   '+'        Plus, match one or more (greedy)
 *   '?'        Question, match zero or one (non-greedy)
 *   '[abc]'    Character class, match if one of {'a', 'b', 'c'}
 *   '[^abc]'   Inverted class, match if NOT one of {'a', 'b', 'c'} -- NOTE: feature is currently broken!
 *   '[a-zA-Z]' Character ranges, the character set of the ranges { a-z | A-Z }
 *   '\s'       Whitespace, \t \f \r \n \v and spaces
 *   '\S'       Non-whitespace
 *   '\w'       Alphanumeric, [a-zA-Z0-9_]
 *   '\W'       Non-alphanumeric
 *   '\d'       Digits, [0-9]
 *   '\D'       Non-digits
 *
 *
 */

#ifndef _TINY_REGEX_C
#define _TINY_REGEX_C


#ifndef RE_DOT_MATCHES_NEWLINE
/* Define to 0 if you DON'T want '.' to match '\r' + '\n' */
#define RE_DOT_MATCHES_NEWLINE 1
#endif

#ifdef __cplusplus
extern "C"{
#endif



/* Typedef'd pointer to get abstract datatype. */
typedef struct regex_t* re_t;


/* Compile regex string pattern to a regex_t-array. */
re_t re_compile(const char* pattern);

/* Find matches of the compiled pattern inside text. */
int re_matchp(re_t pattern, const char* text, int* matchlength);

/* Find matches of the txt pattern inside text (will compile automatically first). */
int re_match(const char* pattern, const char* text, int* matchlength);

char *re_print(regex_t* pattern,char *buf);

#ifdef __cplusplus
}
#endif

class MyRegex {
public:
	enum {
		REGEX_BUF_SIZE=2048
		};
	enum {
		REGEX_FAILURE=0,
		REGEX_SUCCESS=1
		};
public:
//	int Regex(const char *str, const char *pattern, list<string> * groups, bool ignore_case = false);
	int Regex(const char *str, const char *pattern, bool ignore_case = false);
//	int RegexLine(string * str, string * pattern, list<string> * groups, bool ignore_case = false);
	int RegexLine(CString *str, CString *pattern, bool ignore_case = false);

	MyRegex();
	~MyRegex();

protected:
//	const char * Substr(const char *str, int pos1, int pos2);
//	regex_t *pReg;
//	regmatch_t *pMatch;
//	string::size_type NextPos;
//	string *LastStr;
	char RegexBuf[REGEX_BUF_SIZE];

	};



#endif /* ifndef _TINY_REGEX_C */

