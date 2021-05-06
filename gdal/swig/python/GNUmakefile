

include ../../GDALmake.opt

ifndef PYTHON
        PYTHON=python
endif

all: build

BINDING = python
PACKAGE_DIR=osgeo
SWIGOUTPUTDIR=extensions/

include ../SWIGmake.base

PY_COMMANDS     =       epsg_tr.py gdalchksum.py gdal2xyz.py gcps2wld.py \
                        gdalimport.py gdal_merge.py pct2rgb.py rgb2pct.py \
                        gcps2vec.py
PY_MODULES      =       ${PACKAGE_DIR}/gdal.py ${PACKAGE_DIR}/ogr.py ${PACKAGE_DIR}/osr.py ${PACKAGE_DIR}/gdalconst.py ${PACKAGE_DIR}/gdal_array.py ${PACKAGE_DIR}/gnm.py

clean:
	-rm -f ${PACKAGE_DIR}/*.pyc
	-rm -rf build
	-rm -f *.pyc
	-rm -rf *.egg-info
	-rm -f *.so ./osgeo/*.so
	-rm -rf dist

SWIGARGS += -relativeimport -threads
SWIGARGS += -outdir "${PACKAGE_DIR}"


veryclean: clean
	-rm -f .generated_files_up_to_date
	-rm -f ${WRAPPERS} ${PY_MODULES}
	-rm -f $(SWIGOUTPUTDIR)gdal_wrap.cpp
	-rm -f $(SWIGOUTPUTDIR)gdalconst_wrap.c
	-rm -f $(SWIGOUTPUTDIR)ogr_wrap.cpp
	-rm -f $(SWIGOUTPUTDIR)osr_wrap.cpp
	-rm -f $(SWIGOUTPUTDIR)gdal_array_wrap.cpp
	-rm -f $(SWIGOUTPUTDIR)gnm_wrap.cpp

$(SWIGOUTPUTDIR)gdal_array_wrap.cpp:  ../include/gdal_array.i ../include/python/typemaps_python.i
	$(SWIG) $(SWIGARGS) $(SWIGDEFINES) -I$(GDAL_ROOT) -c++ -$(BINDING) -o $@ ../include/gdal_array.i

# Following is to prevent Coverity Scan to warn about Dereference before null check (REVERSE_INULL)
SED_COMMANDS = \
    -e 's/if (!PyTuple_Check(args)) SWIG_fail;/if (args == NULL || !PyTuple_Check(args)) SWIG_fail;/' \
    -e 's/argc = args ? (int)PyObject_Length(args) : 0/argc = (int)PyObject_Length(args)/' \
    -e 's/if (SWIG_IsTmpObj(res\([1-9]\)))/if (ReturnSame(SWIG_IsTmpObj(res\1)))/' \
    -e 's/if( alloc\([1-9]\) == SWIG_NEWOBJ )/if (ReturnSame(alloc\1) == SWIG_NEWOBJ )/' \
    -e 's/strncpy(buff, "swig_ptr: ", 10)/memcpy(buff, "swig_ptr: ", 10)/'
# Unused assigned variable
SED_COMMANDS += -e 's/module_head = \&swig_module;/\/\*module_head = \&swig_module;\*\//'
# SWIG defect fixed per https://github.com/swig/swig/commit/b0e29fbdf31bb94b11cb8a7cc830b4a76467afa3#diff-ba23eb3671d250e6a62261f19f653dd2R93
SED_COMMANDS += -e 's/obj = PyUnicode_AsUTF8String(obj);/obj = PyUnicode_AsUTF8String(obj); if (!obj) return SWIG_TypeError;/'

.generated_files_up_to_date: ${WRAPPERS} $(SWIGOUTPUTDIR)gdal_array_wrap.cpp
	rm -f .generated_files_up_to_date
	if ! grep "CPL_IGNORE_RET_VAL(result" $(SWIGOUTPUTDIR)gdal_wrap.cpp>/dev/null; then \
		for i in gdal_wrap.cpp gdalconst_wrap.c ogr_wrap.cpp osr_wrap.cpp gdal_array_wrap.cpp gnm_wrap.cpp; do \
		    if test -f $(SWIGOUTPUTDIR)$$i; then \
		        sed $(SED_COMMANDS) $(SWIGOUTPUTDIR)$$i > $(SWIGOUTPUTDIR)$$i.tmp; \
		        mv -f $(SWIGOUTPUTDIR)$$i.tmp $(SWIGOUTPUTDIR)$$i; \
		        \
		        if test $$i = "gdal_wrap.cpp" ; then \
		            sed \
		              -e 's/result = (CPLErr)\(.*\);/CPL_IGNORE_RET_VAL(result = (CPLErr)\1);/' \
		              $(SWIGOUTPUTDIR)$$i > $(SWIGOUTPUTDIR)$$i.tmp; \
		            mv -f $(SWIGOUTPUTDIR)$$i.tmp $(SWIGOUTPUTDIR)$$i; \
		        fi; \
		    fi; \
		done; \
	fi
ifeq ($(shell sed --version | grep GNU >/dev/null; echo $$?), 0)
	# Only GNU sed accepts inserting newlines with \n. This is just to please Coverity, so no need to find a workaround
	if ! grep "define SED_HACKS" $(SWIGOUTPUTDIR)gdal_wrap.cpp>/dev/null; then \
		for i in gdal_wrap.cpp gdalconst_wrap.c ogr_wrap.cpp osr_wrap.cpp gnm_wrap.cpp; do \
		    if test -f $(SWIGOUTPUTDIR)$$i; then \
		        sed \
		          -e 's/PyObject \*resultobj = 0;/PyObject \*resultobj = 0; int bLocalUseExceptionsCode = bUseExceptions;/' \
		          -e 's/\#define SWIGPYTHON/\#define SWIGPYTHON\n\#define SED_HACKS/' \
		          -e 's/return resultobj;/if ( ReturnSame(bLocalUseExceptionsCode) ) { CPLErr eclass = CPLGetLastErrorType(); if ( eclass == CE_Failure || eclass == CE_Fatal ) { Py_XDECREF(resultobj); SWIG_Error( SWIG_RuntimeError, CPLGetLastErrorMsg() ); return NULL; } }\n  return resultobj;/' \
		          $(SWIGOUTPUTDIR)$$i > $(SWIGOUTPUTDIR)$$i.tmp; \
		        mv -f $(SWIGOUTPUTDIR)$$i.tmp $(SWIGOUTPUTDIR)$$i; \
		    fi; \
		done; \
	fi
endif
	echo "done" > .generated_files_up_to_date

generate: veryclean
	 $(MAKE) .generated_files_up_to_date

build: .generated_files_up_to_date
	rm -f setup_vars.ini
	echo 'GNM_ENABLED=$(GNM_ENABLED)' >> setup_vars.ini
	$(PYTHON) setup.py build

egg:
	$(PYTHON) setup.py bdist_egg

site_package_dir=$(shell $(PYTHON) -c "from __future__ import print_function;from distutils.sysconfig import get_python_lib;print(get_python_lib(prefix=\"$(PREFIX)\"))")

ifeq ($(PY_HAVE_SETUPTOOLS),1)
    setup_opts=--single-version-externally-managed --record=record.txt
endif

ifdef DESTDIR
    setup_opts+=--root=$(DESTDIR)
endif

ifdef PREFIX
    setup_opts+=--prefix=$(PREFIX)
else ifdef prefix
    ifeq ($(shell uname),Darwin)
        STD_UNIX_LAYOUT=$(shell $(PYTHON) -c "from __future__ import print_function;import sys;print(\"FALSE\" if \"framework\" in sys.prefix.lower() else \"TRUE\")")
        ifeq ($(STD_UNIX_LAYOUT),"TRUE")
            setup_opts+=--prefix=$(prefix)
        else
            ifdef PYTHON_INSTALL_LIB
                setup_opts+=--install-lib=$(PYTHON_INSTALL_LIB)
            endif
        endif
    else
        setup_opts+=--prefix=$(prefix)
    endif
endif

ifdef INSTALL_LAYOUT
    setup_opts+=--install-layout=$(INSTALL_LAYOUT)
else
    ifneq ($(shell echo $(site_package_dir) | grep dist-packages),)
        setup_opts+=--install-layout=deb
    endif
endif

install:
	@if [ ! -d $(site_package_dir) ]; then \
		mkdir -p $(DESTDIR)$(site_package_dir); \
		echo "----------------------------------------------------------------------";\
		echo ""; \
		echo "The GDAL python package has been installed in $(site_package_dir)"; \
		echo "Please ensure to add $(site_package_dir) to your PYTHONPATH"; \
		echo ""; \
		echo "----------------------------------------------------------------------";\
	fi
	env PYTHONPATH=$(site_package_dir)$${PYTHONPATH:+:$$PYTHONPATH} \
		$(PYTHON) setup.py install ${setup_opts}

# see swig/include/python/docs/README to refresh the  ../include/python/docs/ files
docs:
	$(PYTHON) ../include/python/docs/doxy2swig.py ../../xml/ogrlayer_8cpp.xml ../include/python/docs/ogr_layer_docs.i OGRLayerShadow OGR_L_

	$(PYTHON) ../include/python/docs/doxy2swig.py ../../xml/ogrgeometry_8cpp.xml ../include/python/docs/ogr_geometry_docs.i OGRGeometryShadow OGR_G_

	$(PYTHON) ../include/python/docs/doxy2swig.py ../../xml/ogrdatasource_8cpp.xml ../include/python/docs/ogr_datasource_docs.i OGRDataSourceShadow OGR_DS_


	$(PYTHON) ../include/python/docs/doxy2swig.py ../../xml/ogrsfdriver_8cpp.xml ../include/python/docs/ogr_driver_docs.i OGRDriverShadow OGR_Dr_

	$(PYTHON) ../include/python/docs/doxy2swig.py ../../xml/ogrfeature_8cpp.xml ../include/python/docs/ogr_feature_docs.i OGRFeatureShadow OGR_F_

	$(PYTHON) ../include/python/docs/doxy2swig.py ../../xml/ogrfeaturedefn_8cpp.xml ../include/python/docs/ogr_featuredef_docs.i OGRFeatureDefnShadow OGR_FD_

	$(PYTHON) ../include/python/docs/doxy2swig.py ../../xml/ogrfielddefn_8cpp.xml ../include/python/docs/ogr_fielddef_docs.i OGRFieldDefnShadow OGR_Fld_

epydoc: generate
	epydoc --config epydoc.conf

doc-tarball: epydoc
	mv html python
	tar cvzf python-doc.tar.gz python
	rm -rf python

.PHONY: all clean veryclean generate build egg install docs epydoc doc-tarball
