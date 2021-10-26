include ../../GDALmake.opt

HFAOBJ := hfaopen.o hfaentry.o hfadictionary.o hfafield.o hfatype.o
HFAOBJ += hfaband.o hfacompress.o
OBJ := $(HFAOBJ) hfadataset.o hfa_overviews.o

ALL_C_FLAGS = $(CFLAGS)

CPPFLAGS        :=      $(PROJ_INCLUDE) $(CPPFLAGS)

default: $(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o hfatest $(O_OBJ)

$(O_OBJ): hfa.h hfa_p.h

hfatest: hfatest.$(OBJ_EXT) $(HFAOBJ:.o=.$(OBJ_EXT))
	$(LD) hfatest.$(OBJ_EXT) $(HFAOBJ:.o=.$(OBJ_EXT)) ../../port/*.$(OBJ_EXT)  ../../ogr/ogrsf_frmts/o/json_*.o  ../../ogr/ogrsf_frmts/o/printbuf.o  ../../ogr/ogrsf_frmts/o/arraylist.o  ../../ogr/ogrsf_frmts/o/linkhash.o ../../ogr/ograpispy.o ../../ogr/ogr_proj_p.o  -L../.. -lgdal $(LIBS) -o hfatest

install-obj: $(O_OBJ:.o=.$(OBJ_EXT))
