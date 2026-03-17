include ../Makefile.defs
CC=gcc -O2
LIBS=-lpthread
FLAGS=-DCRONO_VERSION=\"$(CRONO_VERSION)\" -DDEFAULT_AMCONF=\"$(DEFAULT_AMCONF)\" \
      -DDIRCONF=\"$(DIRCONF)\" -DLOGIN_ACCESS_FILE=\"$(LOGIN_ACCESS_FILE)\" \
      -DHOSTS_EQUIV_FILE=\"$(HOSTS_EQUIV_FILE)\"


all:	common.o 
clean:
	rm -f *.o

common.o: common.c common.h
	$(CC) $(FLAGS) common.c -c
