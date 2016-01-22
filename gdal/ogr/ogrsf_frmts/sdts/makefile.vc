
OBJ	=	ogrsdtsdatasource.obj \
		ogrsdtsdriver.obj \
		ogrsdtslayer.obj 

EXTRAFLAGS =	-I.. -I..\.. -I$(GDAL_ROOT)/frmts/sdts \
		-I$(GDAL_ROOT)/frmts/iso8211

GDAL_ROOT	=	..\..\..

!INCLUDE $(GDAL_ROOT)\nmake.opt

default:	$(OBJ)

clean:
	-del *.obj *.pdb



