

include ../../GDALmake.opt

HFAOBJ  =	hfaopen.o hfaentry.o hfadictionary.o hfafield.o hfatype.o \
		hfaband.o hfacompress.o
OBJ	=	$(HFAOBJ) hfadataset.o hfa_overviews.o

ALL_C_FLAGS	=	 $(CFLAGS)



default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o hfatest $(O_OBJ)

$(O_OBJ):  hfa.h hfa_p.h

hfatest:	hfatest.$(OBJ_EXT) $(HFAOBJ:.o=.$(OBJ_EXT))
	$(LD) hfatest.$(OBJ_EXT) $(HFAOBJ:.o=.$(OBJ_EXT)) ../../port/*.$(OBJ_EXT) $(LIBS) -o hfatest

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))

