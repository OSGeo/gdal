

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
	for i in gdal_wrap.cpp gdalconst_wrap.c ogr_wrap.cpp osr_wrap.cpp gdal_array_wrap.cpp; do sed "s/PyErr_Format(PyExc_RuntimeError, mesg)/PyErr_SetString(PyExc_RuntimeError, mesg)/" ${SWIGOUTPUTDIR}/$$i | sed "s/int len;/Py_ssize_t len;/" > ${SWIGOUTPUTDIR}/$$i.tmp; mv -f ${SWIGOUTPUTDIR}/$$i.tmp ${SWIGOUTPUTDIR}/$$i; done
	-grep "1\.3\.40" extensions/gdal_wrap.cpp >/dev/null && patch -p0 < python3.2.patch
	-grep "1\.3\.40" extensions/gdal_wrap.cpp >/dev/null && cat python3.2.patch | sed "s/gdal_wrap/ogr_wrap/" | patch -p0
	-grep "1\.3\.40" extensions/gdal_wrap.cpp >/dev/null && cat python3.2.patch | sed "s/gdal_wrap/osr_wrap/" | patch -p0
	-grep "1\.3\.40" extensions/gdal_wrap.cpp >/dev/null && cat python3.2.patch | sed "s/gdal_wrap\.cpp/gdalconst_wrap\.c/" | patch -p0
	-grep "1\.3\.40" extensions/gdal_wrap.cpp >/dev/null && cat python3.2.patch | sed "s/gdal_wrap/gdal_array_wrap/" | patch -p0
    
build:
	$(PYTHON) setup.py build

egg:
	$(PYTHON) setup.py bdist_egg 
	
install:

ifeq ($(PY_HAVE_SETUPTOOLS),1)
	$(PYTHON) setup.py install 
else
	$(PYTHON) setup.py install --prefix=$(DESTDIR)$(prefix)
endif

	for f in $(SCRIPTS) ; do $(INSTALL) ./scripts/$$f $(DESTDIR)$(INST_BIN) ; done

docs:
	$(PYTHON) ../include/python/docs/doxy2swig.py ../../ogr/xml/ogrlayer_8cpp.xml ../include/python/docs/ogr_layer_docs.i OGRLayerShadow OGR_L_

	$(PYTHON) ../include/python/docs/doxy2swig.py ../../ogr/xml/ogrgeometry_8cpp.xml ../include/python/docs/ogr_geometry_docs.i OGRGeometryShadow OGR_G_

	$(PYTHON) ../include/python/docs/doxy2swig.py ../../ogr/xml/ogrdatasource_8cpp.xml ../include/python/docs/ogr_datasource_docs.i OGRDataSourceShadow OGR_DS_


	$(PYTHON) ../include/python/docs/doxy2swig.py ../../ogr/xml/ogrsfdriver_8cpp.xml ../include/python/docs/ogr_driver_docs.i OGRDriverShadow OGR_Dr_

	$(PYTHON) ../include/python/docs/doxy2swig.py ../../ogr/xml/ogrfeature_8cpp.xml ../include/python/docs/ogr_feature_docs.i OGRFeatureShadow OGR_F_

	$(PYTHON) ../include/python/docs/doxy2swig.py ../../ogr/xml/ogrfeaturedefn_8cpp.xml ../include/python/docs/ogr_featuredef_docs.i OGRFeatureDefnShadow OGR_FD_

	$(PYTHON) ../include/python/docs/doxy2swig.py ../../ogr/xml/ogrfielddefn_8cpp.xml ../include/python/docs/ogr_fielddef_docs.i OGRFieldDefnShadow OGR_Fld_

epydoc: generate
	epydoc --config epydoc.conf
