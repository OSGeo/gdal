

ISODIR	=	../../../frmts/iso8211
ISOLIB  =	$(ISODIR)/libiso8211.a

include ../../../GDALmake.opt

OBJ	=	ogrs57driver.o ogrs57datasource.o ogrs57layer.o \
		s57reader.o s57writer.o ddfrecordindex.o \
		s57classregistrar.o s57filecollector.o \
	        s57featuredefns.o

CPPFLAGS	:=	-I.. -I../.. -I$(ISODIR) $(GDAL_INCLUDE) $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o s57dump$(EXE) mks57$(EXE) $(O_OBJ)

all:	default s57dump$(EXE) mks57$(EXE)

$(O_OBJ):	s57.h ogr_s57.h

s57dump$(EXE):	s57dump.$(OBJ_EXT)
	$(LD) $(LDFLAGS) s57dump.$(OBJ_EXT) $(CONFIG_LIBS) -o s57dump$(EXE)
