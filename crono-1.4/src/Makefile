#############################################################################
#  Makefile for CRONO - Version 1.4
#
#  Marco Aurelio Stelmar Netto - stelmar@cpad.pucrs.br 
#############################################################################
include Makefile.defs

##############################################################################
all:
	cd common ; make ; cd ..
	cd am ; make ;cd ..
	cd nm ; make; cd ..
	cd ui ; make; cd ..
	cd rm ; make ;cd ..
	cd tools ; make ; cd ..

clean:
	cd common ; make clean ; cd ..
	cd am ; make clean ; cd ..
	cd nm ; make clean ; cd ..
	cd ui ; make clean ; cd ..
	cd rm ; make clean ; cd ..
	cd tools ; make clean ; cd ..

install:
	if [ ! -d  $(PREF_INSTALL) ] ; then mkdir -p $(PREF_INSTALL) ; fi
	mkdir -p $(PREF_INSTALL)/sbin
	mkdir -p $(PREF_INSTALL)/bin
	cp -p am/cramd rm/crrmd nm/crnmd $(PREF_INSTALL)/sbin/
	cp -p ui/cralloc ui/crnodes ui/crqview ui/crrls ui/crinfo \
         ui/crnmc ui/crsetdef $(PREF_INSTALL)/bin/
	cp -p tools/crstat $(PREF_INSTALL)/sbin/
	cp -p tools/crmkmachfile tools/crrun tools/crcomp tools/crnodesinfofilter\
       $(PREF_INSTALL)/bin/
	mkdir -p $(PREF_INSTALL)/etc/
	mkdir -p $(PREF_INSTALL)/var/queues
	mkdir -p $(PREF_INSTALL)/var/log
	cp -a ../etc/homogeneous $(PREF_INSTALL)/etc/tropical
	cp -a ../etc/amconf $(PREF_INSTALL)/etc/
   
uninstall:
	cd $(PREF_INSTALL)/bin/ ; rm cralloc crqview crrls crinfo crnodes \
        crnmc crsetdef crmkmachfile crnodesinfofilter crrun crcomp
	cd $(PREF_INSTALL)/sbin/ ; rm crstat cramd crrmd crnmd
