
GDAL_ROOT = ..\..

!INCLUDE $(GDAL_ROOT)\nmake.opt

EXTRAFLAGS =	-GX -I$(VCDIR)\ATL\Include 

OBJ	=	com_util.obj
OLE_LIBS =	oledb.lib ole32.lib oleaut32.lib 
ALL_LIBS =	$(OLE_LIBS) $(LIBS)


default:        cinfo.exe

spatialreferenceidl.h: spatialreferenceidl.idl
	midl spatialreferenceidl.idl

cinfo.exe:	CoverageIdl.tlb cinfo.obj $(OBJ)
	cl /Zi cinfo.obj $(OBJ) $(ALL_LIBS)

CoverageIdl.tlb:
	midl -I.. /tlb CoverageIdl.tlb /D MAKE_TLB ..\CoverageIdl.idl
	del CoverageIdl.h CoverageIdl_i.c CoverageIdl_p.c dlldata.c

clean:
	del *.obj
        del *.exe
	del CoverageIdl.tlh
	del CoverageIdl.tli
	del *.ilk
	del *.pdb
	del CoverageIdl.tlb
