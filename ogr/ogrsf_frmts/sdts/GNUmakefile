
GDAL_ROOT	=	../../..

OBJ	=	ogrsdtsdriver.o ogrsdtsdatasource.o ogrsdtslayer.o

FRMTSLIB = 	../ogrsf_frmts.a

ISODIR	=	$(GDAL_ROOT)/frmts/iso8211
ISOLIB  =	$(ISODIR)/libiso8211.a

SDTSDIR = 	$(GDAL_ROOT)/frmts/sdts
SDTSLIB =	$(SDTSDIR)/libsdts_al.a

LINCLUDE =	-I.. -I../.. -I$(GDAL_ROOT)/port -I$(SDTSDIR) -I$(ISODIR)

include $(GDAL_ROOT)/GDALmake.opt

default:	$(OBJ)

clean:
	rm -f *.o

%.o:	%.cpp
	$(CXX) -c $(LINCLUDE) $(CXXFLAGS) $*.cpp -o $*.o
	ar r ../ogrsf_frmts.a $*.o

install-libs:
	install-libs.sh $(ISOLIB) $(FRMTSLIB)
	install-libs.sh $(SDTSLIB) $(FRMTSLIB)

