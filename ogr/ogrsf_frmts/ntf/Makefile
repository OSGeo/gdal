
OBJ	=	ntf_estlayers.obj ntffilereader.obj ntfrecord.obj \
		ogrntfdatasource.obj ogrntfdriver.obj ogrntflayer.obj \
		ogrntffeatureclasslayer.obj ntf_generic.obj ntf_raster.obj \
		ntf_codelist.obj

EXTRAFLAGS =	-I.. -I..\..

GDAL_ROOT	=	..\..\..

!INCLUDE $(GDAL_ROOT)\nmake.opt

default:	$(OBJ)

clean:
	-del *.obj *.pdb



