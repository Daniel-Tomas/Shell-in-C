%{
/*-
 * scanner.l
 * Minishell "lex" source
 * Describes valid input lexic.
 */

#include <unistd.h>     /* isatty, write */
#include <string.h>     /* strdup */

#include "y.tab.h"

static void echo(void)
{
	static int done = 0;
	static int user = 0;

	if (!done) {
		user = isatty(0);
		done = 1;
	}
	if (!user)
		write(2,yytext,yyleng);
}

#define YY_NO_UNPUT

#undef YY_INPUT
#define YY_INPUT(buf,result,max_size) { \
	int c = getchar(); \
	result = (c == EOF) ? YY_NULL : (buf[0] = c, 1); \
}
%}

%option nounput
%option noinput

%%
[ \t]+		{ echo(); }
[|<>&\n]	{ echo(); return *yytext; }
[^ \t|<>&\n]+	{ echo(); yylval.txt = strdup(yytext); return TXT; }
%%
