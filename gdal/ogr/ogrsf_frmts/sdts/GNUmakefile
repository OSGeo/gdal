

include ../../../GDALmake.opt

OBJ	=	ogrsdtsdriver.o ogrsdtsdatasource.o ogrsdtslayer.o

FRMTSLIB = 	../ogr.a

ISODIR	=	../../../frmts/iso8211
ISOLIB  =	$(ISODIR)/libiso8211.a

SDTSDIR = 	../../../frmts/sdts
SDTSLIB =	$(SDTSDIR)/libsdts_al.a

CPPFLAGS :=	-I.. -I../.. -I$(SDTSDIR) -I$(ISODIR) $(GDAL_INCLUDE) $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

$(O_OBJ):       $(SDTSDIR)/sdts_al.h

clean:
	rm -f *.o *-marker $(O_OBJ)

install-libs:	isolib-marker sdtslib-marker
	@echo Done adding ISO8211 and SDTS libraries into $(FRMTSLIB)

isolib-marker:	$(ISOLIB)
	./install-libs.sh $(ISOLIB) $(FRMTSLIB)
	touch isolib-marker

sdtslib-marker:	$(SDTSLIB)
	./install-libs.sh $(SDTSLIB) $(FRMTSLIB)
	touch sdtslib-marker

libs:	
	(cd ../../../frmts/iso8211; $(MAKE))
	(cd ../../../frmts/sdts; $(MAKE))
