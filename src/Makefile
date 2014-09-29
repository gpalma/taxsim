CC=		gcc
#CC=		cc

CFLAGS=		-Wall -Wextra -O3 -fomit-frame-pointer -ffast-math -std=gnu99
#CFLAGS=		-Wall -Wextra -O0 -ggdb -std=gnu99
#DFLAGS=		-DPRGDEBUG


PROG=		taxsim
SOLVER=		util.c types.c graph.c hash_map.c\
		CA.c metric.c tax_sim.c input.c main.c

SOLVEROBJS=	$(SOLVER:.c=.o)
INCLUDES=	-I.
LIBS=		-lm -lpthread
INSTALLDIR=	../

.SUFFIXES:.c .o

all:		$(PROG)

$(PROG):	$(SOLVEROBJS)
		$(CC) $(CFLAGS) $(GVFLAGS) -o $(INSTALLDIR)$(PROG) $(SOLVEROBJS) $(LIBS) $(LDFLAGS)

.c.o:
	$(CC) -c $(INCLUDES) $(CFLAGS) $(DFLAGS) $(GVFLAGS) $< -o $@

.PHONY : clean

clean :
	rm -rf $(INSTALLDIR)$(PROG) *.o *.dSYM *~
