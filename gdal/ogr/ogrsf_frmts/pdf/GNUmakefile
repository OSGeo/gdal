

include ../../../GDALmake.opt

OBJ	=	ogrpdfdriver.o ogrpdfdatasource.o

CPPFLAGS	:=	-I.. -I../.. -I../mem -I../../../frmts/mem -I../../../frmts/pdf $(GDAL_INCLUDE) $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

$(O_OBJ):	ogr_pdf.h ../../../frmts/pdf/pdfobject.h ../../../frmts/pdf/pdfcreatecopy.h