
OBJ	=	dgnopen.obj dgnread.obj dgnfloat.obj \
		ogrdgndriver.obj ogrdgndatasource.obj ogrdgnlayer.obj 

EXTRAFLAGS =	-I.. -I..\..

GDAL_ROOT	=	..\..\..

!INCLUDE $(GDAL_ROOT)\nmake.opt

default:	$(OBJ)

clean:
	-del *.obj *.pdb



