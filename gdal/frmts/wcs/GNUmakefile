
include ../../GDALmake.opt

OBJ	=	wcsdataset.o httpdriver.o gmlcoverage.o wcsutils.o wcsdataset100.o wcsdataset110.o wcsdataset201.o wcsrasterband.o

CPPFLAGS	:=	 -iquote ../../gcore -iquote ../../ogr -iquote ../../ogr/ogrsf_frmts/gml $(CPPFLAGS)

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(OBJ) $(O_OBJ)

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))
