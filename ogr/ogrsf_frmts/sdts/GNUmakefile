
GDAL_ROOT	=	../../..

OBJ	=	ogrsdtsdriver.o ogrsdtsdatasource.o ogrsdtslayer.o

FRMTSLIB = 	../ogr.a

ISODIR	=	$(GDAL_ROOT)/frmts/iso8211
ISOLIB  =	$(ISODIR)/libiso8211.a

SDTSDIR = 	$(GDAL_ROOT)/frmts/sdts
SDTSLIB =	$(SDTSDIR)/libsdts_al.a

LINCLUDE =	-I.. -I../.. -I$(GDAL_ROOT)/port -I$(SDTSDIR) -I$(ISODIR)

include $(GDAL_ROOT)/GDALmake.opt

default:	$(O_OBJ) libs

clean:
	rm -f *.o *-marker

%.o:	%.cpp
	$(CXX) -c $(LINCLUDE) $(CXXFLAGS) $*.cpp -o $*.o

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
