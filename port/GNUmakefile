#
# CPL (Common Portability Library) makefile
#

GDAL_ROOT = ..

ifneq ($(wildcard ../GDALmake.op?),)
include ../GDALmake.opt
else
include GDALmake.opt
endif

OBJ	=	cpl_conv.o cpl_error.o cpl_string.o cpl_vsisimple.o \
		cplgetsymbol.o cpl_dir.o cpl_path.o cpl_csv.o cpl_findfile.o \
		cpl_vsil_unix_stdio_64.o cpl_minixml.o cpl_multiproc.o \
		cpl_list.o

ifeq ($(ODBC_SETTING),yes)
OBJ	:= 	$(OBJ) cpl_odbc.o
endif

default:	$(OBJ)

clean:
	$(RM) *.o $(LIB)

install:
	for f in *.h ; do $(INSTALL_DATA) $$f $(INST_INCLUDE) ; done

xmlreformat:	xmlreformat.o 
	$(CXX) $(CXXFLAGS) xmlreformat.o $(CONFIG_LIBS) -o xmlreformat
