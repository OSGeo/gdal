
VSTUDIO =       F:\VisualStudio

EXTRAFLAGS =	-GX -I$(VSTUDIO)\VC98\ATL\Include 

GDAL_ROOT = F:\GDAL

!INCLUDE $(GDAL_ROOT)\nmake.opt

OBJ	=	com_util.obj
OLE_LIBS =	oledb.lib ole32.lib oleaut32.lib 
ALL_LIBS =	$(OLE_LIBS) $(LIBS)


default:        cinfo.exe

spatialreferenceidl.h: spatialreferenceidl.idl
	midl spatialreferenceidl.idl

cinfo.exe:	cinfo.obj $(OBJ)
	cl /Zi cinfo.obj $(OBJ) $(ALL_LIBS)

clean:
	del *.obj
        del *.exe
