include ../Makefile.defs

CC=gcc -O2
INCLUDE=-I../common/
LIBS=-lpthread
DEFS=-DCRONO_VERSION=\"$(CRONO_VERSION)\" -DDIRCONF=\"$(DIRCONF)\" -DLOGIN_ACCESS_FILE=\"$(LOGIN_ACCESS_FILE)\" \
      -DHOSTS_EQUIV_FILE=\"$(HOSTS_EQUIV_FILE)\"

#FLAGS=-Wall -DDEBUG
DIRTARGET=../../sbin

all:	crnmd 
	cp crnmd $(DIRTARGET)/
clean:
	rm -f *.o crnmd
	rm -f $(DIRTARGET)/crnmd
install: 
	cp crnmd /sbin/

crnmd: crnmd.o ../common/common.o
	$(CC) crnmd.o ../common/common.o -o crnmd $(LIBS) 

crnmd.o: crnmd.c crnmd.h ../common/common.h
	$(CC) $(DEFS) $(FLAGS) $(INCLUDE) crnmd.c -c

