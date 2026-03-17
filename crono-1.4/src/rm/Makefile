include ../Makefile.defs

CC=gcc -O2
INCLUDE=-I../common/ -I./
LIBS=-lpthread -lm 
DEFS=-DCRONO_VERSION=\"$(CRONO_VERSION)\" -DDIRCONF=\"$(DIRCONF)\"

#FLAGS=-Wall -DDEBUG
DIRTARGET=../../sbin

all:	crrmd
	cp crrmd $(DIRTARGET)/ 
clean:
	rm -f *.o crrmd 
	rm -f $(DIRTARGET)/crrmd

crrmd: crrmd.o crrm_schedule.o crrm_em.o crrm_reserve.o ../common/common.o
	$(CC) crrm_em.o crrm_schedule.o crrmd.o crrm_reserve.o ../common/common.o -o crrmd $(LIBS) 

crrmd.o: crrmd.c crrm_schedule.h crrmd.h crrm_reserve.h crrm_em.h \
         ../common/common.h
	$(CC) $(DEFS) $(FLAGS) $(INCLUDE) crrmd.c -c  

crrm_schedule.o: crrm_schedule.c crrm_schedule.h crrmd.h crrm_em.h ../common/common.h
	$(CC) $(DEFS) $(FLAGS) $(INCLUDE) crrm_schedule.c -c 

crrm_em.o: crrm_em.c crrm_em.h crrmd.h ../common/common.h
	$(CC) $(DEFS) $(FLAGS) $(INCLUDE) crrm_em.c -c 

crrm_reserve.o: crrm_reserve.c crrm_reserve.h crrmd.h ../common/common.h
	$(CC) $(DEFS) $(FLAGS) $(INCLUDE) crrm_reserve.c -c 

