include ../Makefile.defs

CC=gcc -O2
INCLUDE= -I../common/ -I../ui/
DEFS=-DCRONO_VERSION=\"$(CRONO_VERSION)\" -DDEFAULT_AMCONF=\"$(DEFAULT_AMCONF)\" \
      -DDIRCONF=\"$(DIRCONF)\" -DLOGIN_ACCESS_FILE=\"$(LOGIN_ACCESS_FILE)\" \
      -DHOSTS_EQUIV_FILE=\"$(HOSTS_EQUIV_FILE)\"


FLAGS=-Wall -DDEBUG
DIRTARGET=../../bin

all:    crmkmachfile crstat crnodesinfofilter 
	cp crmkmachfile crnodesinfofilter crstat $(DIRTARGET)/

clean:
	rm -f *.o crmkmachfile crstat crnodesinfofilter
	cd $(DIRTARGET) ; rm -f crmkmachfile crstat crnodesinfofilter

crmkmachfile: crmkmachfile.o ../ui/crui.o
	$(CC) crmkmachfile.o ../common/common.o ../ui/crui.o -o  crmkmachfile

crnodesinfofilter: crnodesinfofilter.o 
	$(CC) crnodesinfofilter.o -o  crnodesinfofilter

crstat: crstat.o ../ui/crui.o
	$(CC) crstat.o ../common/common.o ../ui/crui.o -o  crstat

%.o: %.c
	$(CC) $(DEFS) $(FLAGS) $(INCLUDE) -c $< -o $@
