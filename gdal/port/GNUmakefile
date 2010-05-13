#
# CPL (Common Portability Library) makefile
#

ifneq ($(wildcard ../GDALmake.op?),)
include ../GDALmake.opt
else
include GDALmake.opt
endif

ifeq ($(LIBZ_SETTING),internal) 
XTRA_OPT =      -I../frmts/zlib 
else 
XTRA_OPT = 
endif 

CPPFLAGS	:= $(CPPFLAGS)	$(CURL_INC) $(XTRA_OPT)

OBJ =	cpl_conv.o cpl_error.o cpl_string.o cplgetsymbol.o \
	cpl_strtod.o cpl_path.o cpl_csv.o cpl_findfile.o cpl_minixml.o \
	cpl_multiproc.o cpl_list.o cpl_getexecpath.o cplstring.o \
	cpl_vsil_win32.o cpl_vsisimple.o cpl_vsil.o cpl_vsi_mem.o \
	cpl_vsil_unix_stdio_64.o cpl_http.o cpl_hash_set.o cplkeywordparser.o \
	cpl_recode_stub.o cpl_quad_tree.o cpl_atomic_ops.o cpl_vsil_subfile.o cpl_time.o \
	cpl_vsil_stdout.o cpl_vsil_sparsefile.o cpl_vsil_abstract_archive.o cpl_vsil_tar.o \
	cpl_vsil_stdin.o cpl_vsil_buffered_reader.o

ifeq ($(ODBC_SETTING),yes)
OBJ	:= 	$(OBJ) cpl_odbc.o
endif


ifeq ($(CURL_SETTING),yes)
CPPFLAGS	:=	$(CPPFLAGS) -DHAVE_CURL
endif

ifneq ($(LIBZ_SETTING),no)
OBJ	:= 	$(OBJ)  cpl_vsil_gzip.o cpl_minizip_ioapi.o cpl_minizip_unzip.o 
CPPFLAGS	:=	$(CPPFLAGS) -DHAVE_LIBZ
endif

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	$(RM) *.o $(LIB)

install:
	for f in *.h ; do $(INSTALL_DATA) $$f $(DESTDIR)$(INST_INCLUDE) ; done

xmlreformat:	xmlreformat.o 
	$(CXX) $(CXXFLAGS) xmlreformat.o $(CONFIG_LIBS) -o xmlreformat
