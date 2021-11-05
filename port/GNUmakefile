#
# CPL (Common Portability Library) makefile
#
ifneq ($(wildcard ../GDALmake.op?),)
include ../GDALmake.opt
else
include GDALmake.opt
endif

XTRA_OPT=

ifeq ($(LIBZ_SETTING),internal)
XTRA_OPT := $(XTRA_OPT) -I../frmts/zlib
endif

ifeq ($(HAVE_BLOSC),yes)
XTRA_OPT := $(XTRA_OPT) -DHAVE_BLOSC
endif

ifeq ($(LIBLZMA_SETTING),yes)
XTRA_OPT := $(XTRA_OPT) -DHAVE_LZMA
endif

ifeq ($(ZSTD_SETTING),yes)
XTRA_OPT := $(XTRA_OPT) -DHAVE_ZSTD
endif

ifeq ($(HAVE_LZ4),yes)
XTRA_OPT := $(XTRA_OPT) -DHAVE_LZ4
endif

CPPFLAGS	:= $(CPPFLAGS)	$(CURL_INC) $(JSON_INCLUDE) $(XTRA_OPT) -DINST_DATA=\"$(INST_DATA)\" -DSYSCONFDIR=\"$(sysconfdir)\"

CXXFLAGS	:=	$(WARN_EFFCPLUSPLUS) $(WARN_OLD_STYLE_CAST) $(CXXFLAGS)

OBJ =	cpl_conv.o cpl_error.o cpl_string.o cplgetsymbol.o cplstringlist.o \
	cpl_strtod.o cpl_path.o cpl_csv.o cpl_findfile.o cpl_minixml.o \
	cpl_multiproc.o cpl_list.o cpl_getexecpath.o cplstring.o \
	cpl_vsil_win32.o cpl_vsisimple.o cpl_vsil.o cpl_vsi_mem.o \
	cpl_vsil_unix_stdio_64.o cpl_http.o cpl_hash_set.o cplkeywordparser.o \
	cpl_recode.o cpl_recode_iconv.o cpl_recode_stub.o cpl_quad_tree.o \
	cpl_atomic_ops.o cpl_vsil_subfile.o cpl_time.o \
	cpl_vsil_stdout.o cpl_vsil_sparsefile.o cpl_vsil_abstract_archive.o \
	cpl_vsil_tar.o cpl_vsil_stdin.o cpl_vsil_buffered_reader.o \
	cpl_base64.o cpl_vsil_curl.o cpl_vsil_curl_streaming.o \
	cpl_vsil_s3.o cpl_vsil_gs.o cpl_vsil_az.o cpl_vsil_adls.o cpl_vsil_oss.o \
	cpl_vsil_swift.o cpl_vsil_webhdfs.o \
	cpl_vsil_cache.o cpl_xml_validate.o cpl_spawn.o \
	cpl_google_oauth2.o cpl_progress.o cpl_virtualmem.o cpl_worker_thread_pool.o \
	cpl_sha1.o cpl_aws.o cpl_aws_win32.cpp cpl_vsi_error.o cpl_cpu_features.o \
	cpl_google_cloud.o cpl_azure.o cpl_alibaba_oss.o cpl_json_streaming_parser.o \
	cpl_json.o cpl_md5.o cpl_swift.o cpl_vsil_plugin.o \
	cpl_vsil_hdfs.o cpl_userfaultfd.o cpl_json_streaming_writer.o \
	cpl_vax.o cpl_vsil_uploadonclose.o cpl_compressor.o cpl_float.o

ifeq ($(ODBC_SETTING),yes)
OBJ	:= 	$(OBJ) cpl_odbc.o
endif


ifeq ($(CURL_SETTING),yes)
CPPFLAGS	:=	$(CPPFLAGS) -DHAVE_CURL
endif

ifneq ($(LIBZ_SETTING),no)
OBJ	:= 	$(OBJ)  cpl_vsil_gzip.o cpl_minizip_ioapi.o \
		cpl_minizip_unzip.o cpl_minizip_zip.o
CPPFLAGS	:=	$(CPPFLAGS) -DHAVE_LIBZ
endif

ifeq ($(HAVE_LIBXML2),yes)
CPPFLAGS	:=	$(CPPFLAGS) $(LIBXML2_INC) -DHAVE_LIBXML2
endif

ifeq ($(HAVE_CRYPTOPP),yes)
CPPFLAGS	:=	$(CPPFLAGS) -DHAVE_CRYPTOPP
endif

ifeq ($(USE_ONLY_CRYPTODLL_ALG),yes)
CPPFLAGS	:=	$(CPPFLAGS) -DUSE_ONLY_CRYPTODLL_ALG
endif

ifeq ($(HAVE_OPENSSL_CRYPTO),yes)
CPPFLAGS	:=	$(CPPFLAGS) -DHAVE_OPENSSL_CRYPTO
endif

ifeq ($(HDFS_ENABLED),yes)
CPPFLAGS	:=	$(CPPFLAGS) $(HDFS_INC) -DHDFS_ENABLED
endif

ifeq ($(ENABLE_UFFD),yes)
CPPFLAGS	:=	$(CPPFLAGS) -DENABLE_UFFD
endif

ifeq ($(LIBDEFLATE_SETTING),yes)
CPPFLAGS 	:=	$(CPPFLAGS) -DHAVE_LIBDEFLATE
endif

default:	$(OBJ:.o=.$(OBJ_EXT)) cpl_sha256.$(OBJ_EXT) cpl_vsil_crypt.$(OBJ_EXT)

# cpl_sha256.cpp and cpl_vsil_crypt.cpp uses cryptocpp headers which make use of
# unsigned integer overflow arithmetic.
CXXFLAGS_NO_SANITIZE_UIO := $(subst -fsanitize=unsigned-integer-overflow,,${CXXFLAGS})

cpl_sha256.$(OBJ_EXT):   cpl_sha256.cpp
	$(CXX) $(GDAL_INCLUDE) $(CXXFLAGS_NO_SANITIZE_UIO) $(CPPFLAGS) -c -o $@ $<

cpl_vsil_crypt.$(OBJ_EXT):   cpl_vsil_crypt.cpp
	$(CXX) $(GDAL_INCLUDE) $(CXXFLAGS_NO_SANITIZE_UIO) $(CPPFLAGS) -c -o $@ $<


$(OBJ): cpl_vsi_virtual.h cpl_vsil_curl_class.h

clean:
	$(RM) *.o $(O_OBJ)

INST_H_FILES = \
	cpl_atomic_ops.h \
	cpl_config_extras.h \
	cpl_conv.h \
	cpl_csv.h \
	cpl_error.h \
	cpl_hash_set.h \
	cpl_http.h \
        cpl_json.h \
	cplkeywordparser.h \
	cpl_list.h \
	cpl_minixml.h \
	cpl_minizip_ioapi.h \
	cpl_minizip_unzip.h \
	cpl_minizip_zip.h \
	cpl_multiproc.h \
	cpl_odbc.h \
	cpl_port.h \
	cpl_progress.h \
	cpl_quad_tree.h \
	cpl_spawn.h \
	cpl_string.h \
	cpl_time.h \
	cpl_virtualmem.h \
	cpl_vsi.h \
	cpl_vsi_error.h \
	cpl_vsi_virtual.h \
	gdal_csv.h \
	cpl_auto_close.h \
	cpl_compressor.h

install:
	for f in $(INST_H_FILES) ; do $(INSTALL_DATA) $$f $(DESTDIR)$(INST_INCLUDE) ; done

xmlreformat:	xmlreformat.o
	$(CXX) $(CXXFLAGS) xmlreformat.o $(CONFIG_LIBS) -o xmlreformat
