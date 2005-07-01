GDAL_ROOT	=	../..

include $(GDAL_ROOT)/GDALmake.opt

OBJ	=	msgdataset.o xritheaderparser.o prologue.o msgcommand.o reflectancecalculator.o

CPPFLAGS	=	$(GDAL_INCLUDE) -I PublicDecompWT/DISE -I PublicDecompWT/COMP/WT/Inc -I PublicDecompWT/COMP/Inc

default:	$(OBJ) waveletlib

waveletlib:
    (cd PublicDecompWT/COMP/WT/Src; $(MAKE))
    (cd PublicDecompWT/COMP/Src; $(MAKE))
    (cd PublicDecompWT/DISE; $(MAKE))

clean:
	rm -f *.o

install-obj:	$(O_OBJ)
