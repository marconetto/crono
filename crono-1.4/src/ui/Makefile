include ../Makefile.defs

CC=gcc -O2
INCLUDE= -I../common/ -I./ 
DEFS=-DCRONO_VERSION=\"$(CRONO_VERSION)\" -DDEFAULT_AMCONF=\"$(DEFAULT_AMCONF)\"

#
#FLAGS= -Wall -DDEBUG
DIRTARGET=../../bin

all:	cralloc crinfo crqview crrls crnodes crnmc crsetdef
	cp cralloc crinfo crqview crrls crnodes crnmc crsetdef $(DIRTARGET)/        
clean:
	rm -f *.o cralloc crinfo crqview crrls crnodes crnmc crsetdef 
	cd $(DIRTARGET) ; rm -f cralloc crinfo crqview crrls crnodes crnmc crsetdef

cralloc: cralloc.o crui.o crui_api.o ../common/common.h 
	$(CC) ../common/common.o crui.o cralloc.o crui_api.o -o cralloc 

crinfo: crinfo.o crui.o crui_api.o ../common/common.h 
	$(CC) crinfo.o ../common/common.o crui.o crui_api.o -o crinfo

crqview: crqview.o crui.o crui_api.o ../common/common.h
	$(CC) crqview.o ../common/common.o crui.o crui_api.o -o crqview 

crrls: crrls.o crui.o crui_api.o ../common/common.h
	$(CC) crrls.o ../common/common.o crui.o crui_api.o -o crrls 

crnodes: crnodes.o crui.o crui_api.o ../common/common.h
	$(CC) crnodes.o ../common/common.o crui.o crui_api.o -o crnodes 

crnmc: crnmc.o crui.o crui_api.o ../common/common.h
	$(CC) crnmc.o ../common/common.o crui.o crui_api.o -o crnmc 

crsetdef: crsetdef.o crui.o ../common/common.h
	$(CC) crsetdef.o ../common/common.o crui.o  -o crsetdef 

# generate object files
cralloc.o: cralloc.c crui.h crui_api.c crui_api.h ../common/common.h
	$(CC) cralloc.c -c $(DEFS) $(FLAGS) $(INCLUDE)

crqview.o: crqview.c crui.h crui_api.c crui_api.h ../common/common.h
	$(CC) crqview.c -c $(DEFS) $(FLAGS) $(INCLUDE)

crinfo.o: crinfo.c crui.h crui_api.c crui_api.h ../common/common.h
	$(CC) crinfo.c -c $(DEFS) $(FLAGS) $(INCLUDE)

crrls.o: crrls.c crui.h crui_api.c crui_api.h ../common/common.h
	$(CC) crrls.c -c $(DEFS) $(FLAGS) $(INCLUDE)

crnodes.o: crnodes.c crui.h crui_api.c crui_api.h ../common/common.h
	$(CC) crnodes.c -c $(DEFS) $(FLAGS) $(INCLUDE)

crnmc.o: crnmc.c crui.h crui_api.c crui_api.h ../common/common.h
	$(CC) crnmc.c -c $(DEFS) $(FLAGS) $(INCLUDE)

crsetdef.o: crsetdef.c crui.h crui_api.c crui_api.h ../common/common.h
	$(CC) crsetdef.c -c $(DEFS) $(FLAGS) $(INCLUDE)

crui_api.o: crui_api.c crui.h crui_api.h ../common/common.h
	$(CC) crui_api.c -c $(DEFS) $(FLAGS) $(INCLUDE)

crui.o: crui.c crui.h crui_api.h ../common/common.h
	$(CC) crui.c -c $(DEFS) $(FLAGS) $(INCLUDE)

