
GDAL_ROOT = 	..\..

EXTRAFLAGS =	-GX -I$(VCDIR)\ATL\Include /W2 -I..

!INCLUDE $(GDAL_ROOT)/nmake.opt

OLE_LIBS =	ole32.lib oleaut32.lib 
GDAL_LIB =	$(GDAL_ROOT)/gdal.lib
LOC_OBJ =       Cover.obj COGRRealGC.obj ComUtility.obj Geometry.obj \
		GridGeometryImpl.obj AffineGeoReferenceImpl.obj \
		GridCoverageFactoryImpl.obj com_util.obj


default:	Cover.dll

Cover.res:	Cover.rc Cover.h
	rc.exe /l 0x1009 /foCover.res /d _DEBUG Cover.rc

Cover.dll:	Cover.h CoverageIdl.h $(LOC_OBJ) Cover.res
	link /debug /dll /def:Cover.def /out:Cover.dll \
		$(LOC_OBJ) $(GDAL_LIB) $(OLE_LIBS) Cover.res

clean:
	del *.obj Cover.dll Cover.h Cover_i.c Cover.tlb Cover.res
	del CoverageIdl.h *.pdb *.ilk *.exp CoverageIdl_i.c
	del CoverageIdl_p.c dlldata.c

Cover.h:      Cover.idl
	midl /tlb Cover.tlb /h Cover.h /IF:/opengis/coverage \
		/Oicf /iid Cover_i.c Cover.idl

CoverageIdl.h:	..\CoverageIdl.idl
	midl /h CoverageIdl.h /I.. ..\CoverageIdl.idl

