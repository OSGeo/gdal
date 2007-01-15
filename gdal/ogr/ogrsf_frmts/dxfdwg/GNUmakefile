

include ../../../GDALmake.opt

OBJ =	ogrdwgdriver.o ogrwritabledwgdatasource.o OdFileBuf.o \
	ogrwritabledwglayer.o

DWG_INC = 	-I$(DWGDIRECT)/Include -I$(DWGDIRECT)/Include/Gi \
	-I$(DWGDIRECT)/Include/Ge -I$(DWGDIRECT)/Include/Br \
	-I$(DWGDIRECT)/Include/Gs -I$(DWGDIRECT)/Include/ModelerGeometry

CPPFLAGS :=	-I.. -I../.. \
		$(GDAL_INCLUDE) $(DWG_INC) $(CPPFLAGS) 

default:	$(O_OBJ)

clean:
	rm -f *.o $(O_OBJ)

$(OBJ):	ogr_dwg.h

ODREADEX_SRC =	OdReadExSrc/DbDumper.cpp \
		OdReadExSrc/ExProtocolExtension.cpp \
		OdReadExSrc/GiWorldDrawDumper.cpp \
		OdReadExSrc/OdFileBuf.cpp \
		OdReadExSrc/OdReadEx.cpp \
		\
		OdReadExSrc/ExDbCommandContext.cpp \
		OdReadExSrc/ExHostAppServices.cpp \
		OdReadExSrc/ExPageController.cpp \
		OdReadExSrc/ExStringIO.cpp \
		OdReadExSrc/ExSystemServices.cpp \
		OdReadExSrc/ExUndoController.cpp 

OdReadEx:	$(ODREADEX_SRC)
	$(CXX) -Wno-deprecated $(CPPFLAGS) $(CXXFLAGS) -IOdReadExSrc \
		$(ODREADEX_SRC) $(DWG_LIBS) -o OdReadEx

ODWRITEEX_SRC =	OdWriteExSrc/DbFiller.cpp \
		OdReadExSrc/ExProtocolExtension.cpp \
		OdReadExSrc/GiWorldDrawDumper.cpp \
		OdReadExSrc/OdFileBuf.cpp \
		OdWriteExSrc/OdWriteEx.cpp \
		\
		OdReadExSrc/ExDbCommandContext.cpp \
		OdReadExSrc/ExHostAppServices.cpp \
		OdReadExSrc/ExPageController.cpp \
		OdReadExSrc/ExStringIO.cpp \
		OdReadExSrc/ExSystemServices.cpp \
		OdReadExSrc/ExUndoController.cpp 

OdWriteEx:	$(ODWRITEEX_SRC)
	$(CXX) -Wno-deprecated $(CPPFLAGS) $(CXXFLAGS) -IOdWriteExSrc \
		-IOdReadExSrc \
		$(ODWRITEEX_SRC) $(DWG_LIBS) -o OdWriteEx
