
GDAL_ROOT	=	../../..

include $(GDAL_ROOT)/GDALmake.opt

OBJ	=	ogrmysqldriver.o ogrmysqldatasource.o \
		ogrmysqltablelayer.o ogrmysqllayer.o ogrmysqlresultlayer.o

CPPFLAGS	:=	-I.. -I../.. $(GDAL_INCLUDE) $(MYSQL_INC) $(CPPFLAGS)

default:	$(O_OBJ)

clean:
	rm -f *.o $(O_OBJ)
