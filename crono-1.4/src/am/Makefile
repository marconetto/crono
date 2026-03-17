include ../Makefile.defs

CC=gcc -O2 
INCLUDE=-I../common/
LIBS=-lpthread
DEFS=-DCRONO_VERSION=\"$(CRONO_VERSION)\" -DDIRCONF=\"$(DIRCONF)\"
#FLAGS=-DDEBUG -Wall 
DIRTARGET=../../sbin/
   
all:	cramd 
	cp cramd $(DIRTARGET)   
clean:
	rm -f *.o cramd
	rm -f $(DIRTARGET)cramd 

cramd: cramd.o cram_parsers.o ../common/common.o
	$(CC) cramd.o ../common/common.o cram_parsers.o -o cramd $(LIBS) 

cramd.o: cramd.c cram_parsers.h cramd.h ../common/common.h
	$(CC) $(DEFS) $(FLAGS) $(INCLUDE) cramd.c -c 

cram_parsers.o: cram_parsers.c cram_parsers.h cramd.h ../common/common.h
	$(CC) $(DEFS) $(FLAGS) $(INCLUDE) cram_parsers.c -c 
