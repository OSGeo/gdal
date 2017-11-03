
include ../../GDALmake.opt

OBJ	=	httpdriver.o gmlcoverage.o wcsutils.o wcsdataset.o wcsdataset100.o wcsdataset110.o wcsdataset201.o wcsrasterband.o

CPPFLAGS	:=	 -I../../gcore -I../../ogr -I../../ogr/ogrsf_frmts/gml $(CPPFLAGS)

#default:	$(OBJ:.o=.$(OBJ_EXT))
default:	$(OBJ)

clean:
	rm -f *.o $(OBJ) $(O_OBJ)

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))

wcsutils.o:	wcsutils.cpp wcsutils.h

wcsdataset.o:	wcsdataset.h wcsdataset.cpp wcsutils.h

wcsdataset100.o:	wcsdataset.h wcsdataset100.cpp wcsutils.h

wcsdataset110.o:	wcsdataset.h wcsdataset110.cpp wcsutils.h

wcsdataset201.o:	wcsdataset.h wcsdataset201.cpp wcsutils.h

wcsrasterband.o:	wcsdataset.h wcsrasterband.h wcsrasterband.cpp

httpdriver.o:	httpdriver.cpp

gmlcoverage.o:	gmlcoverage.cpp
