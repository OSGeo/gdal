

include ../../../GDALmake.opt

MIN_OBJ	=	dgnopen.o dgnread.o dgnfloat.o dgnhelp.o dgnwrite.o dgnstroke.o
OBJ =	ogrdgndriver.o ogrdgndatasource.o ogrdgnlayer.o $(MIN_OBJ)

CPPFLAGS	:=	-I.. -I../.. $(GDAL_INCLUDE) $(CPPFLAGS)

CPL_DIR	=	../../../port

WEB_DIR =	$(HOME)/dgnlib-web
DIST_REV =	1.11
DIST_DIR =	dgnlib-$(DIST_REV)
DIST_FILE = 	dgnlib-$(DIST_REV).zip
DIST_FILES =	dgnread.cpp dgnopen.cpp dgnlib.h dgnlibp.h dgnfloat.cpp \
		dgnstroke.cpp dgnwrite.cpp dgnhelp.cpp \
		dgnwritetest.c dgndump.c \
		\
		dist/Makefile dist/Makefile.vc dist/cpl_config.h \
		dist/README \
		\
		$(CPL_DIR)/cpl_port.h \
		$(CPL_DIR)/cpl_vsi.h $(CPL_DIR)/cpl_vsisimple.cpp \
		$(CPL_DIR)/cpl_conv.h $(CPL_DIR)/cpl_conv.cpp \
		$(CPL_DIR)/cpl_error.h $(CPL_DIR)/cpl_error.cpp \
		$(CPL_DIR)/cpl_string.h $(CPL_DIR)/cpl_string.cpp \
		$(CPL_DIR)/cpl_config.h.vc $(CPL_DIR)/cpl_vsil_simple.cpp \
		$(CPL_DIR)/cpl_path.cpp $(CPL_DIR)/cpl_dir.cpp \
		$(CPL_DIR)/cpl_multiproc.h $(CPL_DIR)/cpl_multiproc.cpp

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -rf *.o $(DEST_DIR) dgnlib-$(DIST_REV).zip html man $(O_OBJ)
	rm -f dgndump$(EXE) dgnwritetest$(EXE)

dgndump$(EXE):	dgndump.$(OBJ_EXT)
	$(LD) $(LDFLAGS) dgndump.$(OBJ_EXT) $(CONFIG_LIBS) -o dgndump$(EXE)

dgnwritetest$(EXE):	dgnwritetest.$(OBJ_EXT)
	$(LD) $(LDFLAGS) dgnwritetest.$(OBJ_EXT) $(CONFIG_LIBS) -o dgnwritetest$(EXE)

docs:
	doxygen

zip:
	rm -rf $(DIST_DIR)
	mkdir $(DIST_DIR)
	cp $(DIST_FILES) $(DIST_DIR)
	zip -r $(DIST_FILE) $(DIST_DIR)

web-docs-update:	docs
	cp html/* $(WEB_DIR)/libhtml

web-update:	web-docs-update dist
	cp $(DIST_FILE) $(WEB_DIR)/dl

pge_test:	pge_test.cpp dgn_pge.cpp $(OBJ)
	gcc -Wall -g pge_test.cpp dgn_pge.cpp $(OBJ) $(GDAL_LIB) -lm -o pge_test
