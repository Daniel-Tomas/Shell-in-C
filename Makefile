#
# Makefile
# Minishell "make" source
# Regenerate msh recompiling only modified sources.


CC	= gcc
CFLAGS	= -Wall -g
YACC	= bison --yacc
YFLAGS	= -d
LEX	= flex
LFLAGS	=

OBJS	= parser.o scanner.o main.o

all: msh

msh: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS)

clean:
	rm -f *.tab.? *.o *.bak *~ parser.c scanner.c core msh

cleanall: clean
	rm -f :* freefds* nofiles* killmyself* sigdfl*

depend:
	makedepend main.c parser.y scanner.l

