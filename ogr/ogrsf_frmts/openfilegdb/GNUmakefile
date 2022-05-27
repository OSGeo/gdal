

include ../../../GDALmake.opt

OBJ	=	ogropenfilegdbdriver.o \
		ogropenfilegdbdatasource.o ogropenfilegdbdatasource_write.o \
		ogropenfilegdblayer.o ogropenfilegdblayer_write.o \
		filegdbtable.o filegdbtable_write.o filegdbindex.o filegdbindex_write.o \
		filegdbtable_write_fields.o filegdbtable_freelist.o

CPPFLAGS	:=	-iquote .. -iquote ../.. -iquote ../mem  $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

$(O_OBJ):	ogr_openfilegdb.h filegdbtable.h filegdbtable_priv.h filegdb_fielddomain.h ../../ogr_swq.h ../mem/ogr_mem.h
