

ISODIR	=	../../../frmts/iso8211
ISOLIB  =	$(ISODIR)/libiso8211.a

include ../../../GDALmake.opt

OBJ	=	ogrs57driver.o ogrs57datasource.o ogrs57layer.o \
		s57reader.o s57writer.o ddfrecordindex.o \
		s57classregistrar.o s57filecollector.o \
	        s57featuredefns.o

CPPFLAGS	:=	-I.. -I../.. -I$(ISODIR) $(GDAL_INCLUDE) $(CPPFLAGS)

#LIBS	:=	$(GDAL_LIB) $(ISOLIB) $(LIBS) 
LIBS	:=	$(GDAL_LIB) $(LIBS) 

default:	$(O_OBJ)

clean:
	rm -f *.o s57dump mks57 $(O_OBJ)

all:	default s57dump mks57

s57dump:	s57dump.cpp $(OBJ)
	$(CXX) $(CPPFLAGS) $(CFLAGS) s57dump.cpp $(OBJ) $(LIBS) -o s57dump

mks57:	mks57.cpp $(OBJ)
	$(CXX) $(CPPFLAGS) $(CFLAGS) mks57.cpp $(OBJ) $(LIBS) -o mks57

write_soundg:	write_soundg.cpp $(OBJ)
	$(CXX) $(CPPFLAGS) $(CFLAGS) write_soundg.cpp $(OBJ) $(LIBS) -o write_soundg
