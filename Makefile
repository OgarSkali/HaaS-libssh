# $Id: Makefile,v 1.6 2018/01/19 20:01:35 skalak Exp $

TARGET = haas

SOURCES=$(wildcard src/*.c)
OBJS=$(SOURCES:src/%.c=obj/%.o)

TEMPS=$(OBJS:.o=.i) $(OBJS:.o=.s) $(OBJS:.o=.d)

CFLAGS += -g3 -Wall -Wno-pointer-sign
LDFLAGS +=
LIBS += -lssh

ifdef LIBSSHDEVEL
LIBSSHPATH = /home/ogar/work/HaaS/libssh/local
#CFLAGS += -isysroot $(LIBSSHDEVEL)/include
CFLAGS += -B$(LIBSSHPATH)/include
# -I$(LIBSSHDEVEL)/include
LDFLAGS += -Wl,-rpath=$(LIBSSHPATH)/lib -L$(LIBSSHPATH)/lib 
# --sysroot=$(LIBSSHDEVEL)/lib
# -L$(LIBSSHDEVEL)/lib

endif # LIBSSHDEVEL

################################################################################

.PHONY: all clean vacuum dist

all: $(TARGET)

$(TARGET): obj $(OBJS)
	$(CC)  $(OBJS) $(LDFLAGS) $(LIBS) -o $@

obj:
	mkdir obj

clean:
	$(RM) -f $(TARGET) core* $(OBJS)

vacuum: clean
	$(RM) $(TEMPS)

dist:
	tar zcvf $(shell date +%Y%m%d%H%M%S).tgz src/*.[ch] Makefile .cvsignore

################################################################################

.SUFFIXES: 
.SUFFIXES: .c .o

obj/%.o: src/%.c
	$(CC) -MMD -MP -c $(CFLAGS) $< -o $@

-include $(OBJS:.o=.d)
