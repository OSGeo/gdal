
GDAL_ROOT	=	../../..

ISODIR	=	$(GDAL_ROOT)/frmts/iso8211
ISOLIB  =	$(ISODIR)/libiso8211.a

include $(GDAL_ROOT)/GDALmake.opt

OBJ	=	ogrs57driver.o ogrs57datasource.o ogrs57layer.o \
		s57reader.o ddfrecordindex.o \
		s57classregistrar.o s57filecollector.o

ALL_C_FLAGS	=	-I.. -I../.. -I$(GDAL_ROOT)/port -I$(ISODIR) $(CFLAGS)

LIBS	:=	$(GDAL_LIB) $(ISOLIB) $(LIBS) 


default:	$(O_OBJ)

clean:
	rm -f *.o

%.o:	%.cpp
	$(CXX) -c $(ALL_C_FLAGS) $*.cpp -o $*.o

%.o:	%.c
	$(CC) -c $(ALL_C_FLAGS) $*.c -o $*.o

s57dump:	s57dump.cpp $(OBJ)
	$(CXX) $(ALL_C_FLAGS) s57dump.cpp $(OBJ) $(LIBS) -o s57dump
