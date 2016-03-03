

include ../../GDALmake.opt

ifndef PYTHON
        PYTHON=python
endif

all: build

BINDING = python
include ../SWIGmake.base

PACKAGE_DIR=osgeo
SWIGOUTPUTDIR=extensions/

SCRIPTS			= `ls ./scripts`
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

SWIGARGS += -outdir "${PACKAGE_DIR}" 


veryclean: clean
	-rm -f ${WRAPPERS} ${PY_MODULES}
	-rm -f ${SWIGOUTPUTDIR}/gdal_wrap.cpp 
	-rm -f ${SWIGOUTPUTDIR}/gdalconst_wrap.c
	-rm -f ${SWIGOUTPUTDIR}/ogr_wrap.cpp
	-rm -f ${SWIGOUTPUTDIR}/osr_wrap.cpp
	-rm -f ${SWIGOUTPUTDIR}/gdal_array_wrap.cpp
	-rm -f ${SWIGOUTPUTDIR}/gnm_wrap.cpp

gdal_wrap.cpp: ../include/python/gdal_python.i

ogr_wrap.cpp: ../include/python/ogr_python.i

osr_wrap.cpp: ../include/python/osr_python.i

gdal_array_wrap.cpp:  ../include/gdal_array.i ../include/python/typemaps_python.i
	$(SWIG) $(SWIGARGS) $(SWIGDEFINES) -I$(GDAL_ROOT) -c++ -$(BINDING) -o $(SWIGOUTPUTDIR)$@ gdal_array.i

# A few hacks (cat, mv) : the first one for SWIG < 1.3.36 and the second one for SWIG <= 1.3.39 python 3.X on 64bit platforms
# The python3.2.patch is from https://sourceforge.net/tracker/?func=detail&aid=3057804&group_id=1645&atid=101645
# and is no longer necessary with swig 2.0.4
generate: ${WRAPPERS} gdal_array_wrap.cpp
	for i in gdal_wrap.cpp gdalconst_wrap.c ogr_wrap.cpp osr_wrap.cpp gdal_array_wrap.cpp gnm_wrap.cpp; do if test -f ${SWIGOUTPUTDIR}/$$i; then sed "s/PyErr_Format(PyExc_RuntimeError, mesg)/PyErr_SetString(PyExc_RuntimeError, mesg)/" ${SWIGOUTPUTDIR}/$$i | sed "s/int len;/Py_ssize_t len;/" > ${SWIGOUTPUTDIR}/$$i.tmp; mv -f ${SWIGOUTPUTDIR}/$$i.tmp ${SWIGOUTPUTDIR}/$$i; fi; done
	# Following is to prevent Coverity Scan to warn about Dereference before null check (REVERSE_INULL)
	for i in gdal_wrap.cpp gdalconst_wrap.c ogr_wrap.cpp osr_wrap.cpp gdal_array_wrap.cpp gnm_wrap.cpp; do if test -f ${SWIGOUTPUTDIR}/$$i; then sed "s/if (!PyTuple_Check(args)) SWIG_fail;/if (args == NULL || !PyTuple_Check(args)) SWIG_fail;/" ${SWIGOUTPUTDIR}/$$i | sed "s/argc = args ? (int)PyObject_Length(args) : 0/argc = (int)PyObject_Length(args)/" > ${SWIGOUTPUTDIR}/$$i.tmp; mv -f ${SWIGOUTPUTDIR}/$$i.tmp ${SWIGOUTPUTDIR}/$$i; fi; done
	for i in gdal_wrap.cpp gdalconst_wrap.c ogr_wrap.cpp osr_wrap.cpp gdal_array_wrap.cpp gnm_wrap.cpp; do if test -f ${SWIGOUTPUTDIR}/$$i; then sed "s/if (SWIG_IsTmpObj(res\([1-9]\)))/if (ReturnSame(SWIG_IsTmpObj(res\1)))/" ${SWIGOUTPUTDIR}/$$i  > ${SWIGOUTPUTDIR}/$$i.tmp; mv -f ${SWIGOUTPUTDIR}/$$i.tmp ${SWIGOUTPUTDIR}/$$i; fi; done
	for i in gdal_wrap.cpp gdalconst_wrap.c ogr_wrap.cpp osr_wrap.cpp gdal_array_wrap.cpp gnm_wrap.cpp; do if test -f ${SWIGOUTPUTDIR}/$$i; then sed "s/if( alloc\([1-9]\) == SWIG_NEWOBJ )/if (ReturnSame(alloc\1) == SWIG_NEWOBJ )/" ${SWIGOUTPUTDIR}/$$i  > ${SWIGOUTPUTDIR}/$$i.tmp; mv -f ${SWIGOUTPUTDIR}/$$i.tmp ${SWIGOUTPUTDIR}/$$i; fi; done
	for i in gdal_wrap.cpp gdalconst_wrap.c ogr_wrap.cpp osr_wrap.cpp gdal_array_wrap.cpp gnm_wrap.cpp; do if test -f ${SWIGOUTPUTDIR}/$$i; then sed 's/strncpy(buff, "swig_ptr: ", 10)/memcpy(buff, "swig_ptr: ", 10)/' ${SWIGOUTPUTDIR}/$$i  > ${SWIGOUTPUTDIR}/$$i.tmp; mv -f ${SWIGOUTPUTDIR}/$$i.tmp ${SWIGOUTPUTDIR}/$$i; fi; done
	# Only GNU sed accepts inserting newlines with \n. This is just to please Coverity, so no need to find a workaround
	sed --version | grep GNU >/dev/null && (for i in gdal_wrap.cpp gdalconst_wrap.c ogr_wrap.cpp osr_wrap.cpp gnm_wrap.cpp; do if test -f ${SWIGOUTPUTDIR}/$$i; then sed "s/PyObject \*resultobj = 0;/PyObject \*resultobj = 0; int bLocalUseExceptionsCode = bUseExceptions;/" ${SWIGOUTPUTDIR}/$$i  | sed "s/#define SWIGPYTHON/#define SWIGPYTHON\n#define SED_HACKS/" | sed "s/return resultobj;/if ( ReturnSame(bLocalUseExceptionsCode) ) { CPLErr eclass = CPLGetLastErrorType(); if ( eclass == CE_Failure || eclass == CE_Fatal ) { Py_XDECREF(resultobj); SWIG_Error( SWIG_RuntimeError, CPLGetLastErrorMsg() ); return NULL; } }\n  return resultobj;/" > ${SWIGOUTPUTDIR}/$$i.tmp; mv -f ${SWIGOUTPUTDIR}/$$i.tmp ${SWIGOUTPUTDIR}/$$i; fi; done) || /bin/true
	# Unused assigned variable
	for i in gdal_wrap.cpp gdalconst_wrap.c ogr_wrap.cpp osr_wrap.cpp gdal_array_wrap.cpp gnm_wrap.cpp; do if test -f ${SWIGOUTPUTDIR}/$$i; then sed 's/module_head = \&swig_module;/\/\*module_head = \&swig_module;\*\//' ${SWIGOUTPUTDIR}/$$i  > ${SWIGOUTPUTDIR}/$$i.tmp; mv -f ${SWIGOUTPUTDIR}/$$i.tmp ${SWIGOUTPUTDIR}/$$i; fi; done
	for i in gdal_wrap.cpp; do sed 's/result = (CPLErr)\(.*\);/CPL_IGNORE_RET_VAL(result = (CPLErr)\1);/' ${SWIGOUTPUTDIR}/$$i  > ${SWIGOUTPUTDIR}/$$i.tmp; mv -f ${SWIGOUTPUTDIR}/$$i.tmp ${SWIGOUTPUTDIR}/$$i; done
	-grep "1\.3\.40" extensions/gdal_wrap.cpp >/dev/null && patch -p0 < python3.2.patch
	-grep "1\.3\.40" extensions/gdal_wrap.cpp >/dev/null && cat python3.2.patch | sed "s/gdal_wrap/ogr_wrap/" | patch -p0
	-grep "1\.3\.40" extensions/gdal_wrap.cpp >/dev/null && cat python3.2.patch | sed "s/gdal_wrap/osr_wrap/" | patch -p0
	-grep "1\.3\.40" extensions/gdal_wrap.cpp >/dev/null && cat python3.2.patch | sed "s/gdal_wrap\.cpp/gdalconst_wrap\.c/" | patch -p0
	-grep "1\.3\.40" extensions/gdal_wrap.cpp >/dev/null && cat python3.2.patch | sed "s/gdal_wrap/gdal_array_wrap/" | patch -p0

build: extensions/gdal_wrap.cpp
	rm -f setup_vars.ini
	echo 'GNM_ENABLED=$(GNM_ENABLED)' >> setup_vars.ini
	$(PYTHON) setup.py build

egg:
	$(PYTHON) setup.py bdist_egg 

site_package_dir=$(shell $(PYTHON) -c "from __future__ import print_function;from distutils.sysconfig import get_python_lib;print(get_python_lib(prefix=\"$(prefix)\"))")

ifeq ($(PY_HAVE_SETUPTOOLS),1)
    setup_opts=--single-version-externally-managed --record=record.txt
endif

ifdef DESTDIR
    setup_opts+=--root=$(DESTDIR)
endif

ifdef prefix
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
	for f in $(SCRIPTS) ; do $(INSTALL) ./scripts/$$f $(DESTDIR)$(INST_BIN) ; done

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
