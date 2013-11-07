

include ../../../GDALmake.opt

OBJ	=	ogrmysqldriver.o ogrmysqldatasource.o \
		ogrmysqltablelayer.o ogrmysqllayer.o ogrmysqlresultlayer.o

CPPFLAGS	:=	-I.. -I../.. $(GDAL_INCLUDE) $(MYSQL_INC) $(CPPFLAGS)

ifeq ($(MYSQL_NEEDS_LOAD_DEFAULTS_DECLARATION),yes)
CPPFLAGS +=   -DMYSQL_NEEDS_LOAD_DEFAULTS_DECLARATION
endif


default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

$(O_OBJ):	ogr_mysql.h