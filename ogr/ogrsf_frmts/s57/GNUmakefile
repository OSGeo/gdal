
GDAL_ROOT	=	../../..

ISODIR	=	$(GDAL_ROOT)/frmts/iso8211
ISOLIB  =	$(ISODIR)/libiso8211.a

include $(GDAL_ROOT)/GDALmake.opt

OBJ	=	ogrs57driver.o ogrs57datasource.o ogrs57layer.o \
		s57reader.o ddfrecordindex.o \
		s57classregistrar.o s57filecollector.o

CPPFLAGS	+=	-I.. -I../.. -I$(ISODIR)

LIBS	:=	$(GDAL_LIB) $(ISOLIB) $(LIBS) 

default:	$(O_OBJ)

clean:
	rm -f *.o s57dump

s57dump:	s57dump.cpp $(OBJ)
	$(CXX) $(CPPFLAGS) $(CFLAGS) s57dump.cpp $(OBJ) $(LIBS) -o s57dump
