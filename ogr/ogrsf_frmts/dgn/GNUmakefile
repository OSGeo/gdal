
GDAL_ROOT	=	../../..

include $(GDAL_ROOT)/GDALmake.opt

OBJ	=	dgnopen.o dgnread.o dgnfloat.o dgnhelp.o dgnwrite.o dgnstroke.o
OBJ_OGR =	ogrdgndriver.o ogrdgndatasource.o ogrdgnlayer.o

ALL_C_FLAGS	=	-I.. -I../.. -I$(GDAL_ROOT)/port $(CFLAGS)

LIBS	:=	../ogrsf_frmts.a ../../ogr.a $(CPL_LIB) $(LIBS) 

CPL_DIR	=	$(GDAL_ROOT)/port

DIST_REV =	1.7
DIST_DIR =	dgnlib-$(DIST_REV)
DIST_FILE = 	dgnlib-$(DIST_REV).zip
DIST_FILES =	dgnread.cpp dgnopen.cpp dgnlib.h dgnlibp.h dgnfloat.cpp \
		dgnstroke.cpp dgnwrite.cpp dgnhelp.cpp README \
		dgnwritetest.c dgndump.c \
		\
		$(CPL_DIR)/cpl_config.h $(CPL_DIR)/cpl_port.h \
		$(CPL_DIR)/cpl_vsi.h $(CPL_DIR)/cpl_vsisimple.cpp \
		$(CPL_DIR)/cpl_conv.h $(CPL_DIR)/cpl_conv.cpp \
		$(CPL_DIR)/cpl_error.h $(CPL_DIR)/cpl_error.cpp \
		$(CPL_DIR)/cpl_string.h $(CPL_DIR)/cpl_string.cpp \
		$(CPL_DIR)/cpl_config.h.vc

default:	$(O_OBJ) $(foreach file,$(OBJ_OGR),../o/$(file))

clean:
	rm -rf *.o $(DEST_DIR) dgnlib-$(DIST_REV).zip html man dgndump

%.o:	%.cpp
	$(CXX) -c $(ALL_C_FLAGS) $*.cpp -o $*.o
	ar r ../ogrsf_frmts.a $*.o

%.o:	%.c
	$(CC) -c $(ALL_C_FLAGS) $*.c -o $*.o
	ar r ../ogrsf_frmts.a $*.o

dgndump:	dgndump.c $(OBJ)
	$(CXX) $(ALL_C_FLAGS) dgndump.c $(OBJ) $(CPL_LIB) -ldl -lm -o dgndump 

dgnwritetest:	dgnwritetest.c $(OBJ)
	$(CXX) $(ALL_C_FLAGS) dgnwritetest.c $(OBJ) $(CPL_LIB) -ldl -lm \
	-o dgnwritetest

docs:
	doxygen

dist:
	rm -rf $(DIST_DIR)
	mkdir $(DIST_DIR)
	cp $(DIST_FILES) $(DIST_DIR)
	cp Makefile.dist $(DIST_DIR)/Makefile
	cp Makefile.vc.dist $(DIST_DIR)/Makefile.vc
	zip -r $(DIST_FILE) $(DIST_DIR)

web-docs-update:	docs
	cp html/* /u/www/projects/dgn/libhtml

web-update:	web-docs-update dist
	cp $(DIST_FILE) /u/www/projects/dgn/


pge_test:	pge_test.cpp dgn_pge.cpp $(OBJ)
	gcc -Wall -g pge_test.cpp dgn_pge.cpp $(OBJ) $(GDAL_HOME)/port/cpl.a -lm -o pge_test


