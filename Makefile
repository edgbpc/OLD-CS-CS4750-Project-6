# Makefile with suffix rules

CC	= gcc
CFLAGS	= -g -lrt -lpthread
TARGET1 = oss
TARGET2 = user
OBJS1 	= oss.o oss.h
OBJS2 	= user.o oss.h
LIBOBJS =
LIBS =
MYLIBS =


.SUFFIXES: .c .o


all: $(TARGET1) $(TARGET2)

oss: $(OBJS1)
	$(CC) $(CFLAGS) $(OBJS1) -o $@


user: $(OBJS2)
	  $(CC) $(CFLAGS) $(OBJS2) -o $@

.c.o:
	$(CC) $(CFLAGS) -c $<

.PHONY: clean
clean:
	/bin/rm -f *.log *.o *~ $(TARGET1) $(TARGET2)

