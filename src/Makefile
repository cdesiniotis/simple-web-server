CC		= gcc
DEBUG		= 0
CFLAGS		= -D DEBUG=$(DEBUG)
OBJS		= str.o net.o http.o server.o
PROG		= server

.PHONY:		all clean

all:		$(PROG)

$(PROG):	$(OBJS)
		$(CC) -o $(PROG) $(OBJS)

clean:;		$(RM) $(PROG) core *.o

