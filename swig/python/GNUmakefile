

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
PY_MODULES      =       ${PACKAGE_DIR}/gdal.py ${PACKAGE_DIR}/ogr.py ${PACKAGE_DIR}/osr.py ${PACKAGE_DIR}/gdalconst.py

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

gdal_wrap.cpp: ../include/python/gdal_python.i

ogr_wrap.cpp: ../include/python/ogr_python.i

osr_wrap.cpp: ../include/python/osr_python.i

# Remove the following hack (cat, mv) as soon we have upgraded to SWIG >= 1.3.36
generate: ${WRAPPERS}
	sed "s/PyErr_Format(PyExc_RuntimeError, mesg)/PyErr_SetString(PyExc_RuntimeError, mesg)/" ${SWIGOUTPUTDIR}/gdal_wrap.cpp > ${SWIGOUTPUTDIR}/gdal_wrap.cpp.tmp
	mv -f ${SWIGOUTPUTDIR}/gdal_wrap.cpp.tmp ${SWIGOUTPUTDIR}/gdal_wrap.cpp
	sed "s/PyErr_Format(PyExc_RuntimeError, mesg)/PyErr_SetString(PyExc_RuntimeError, mesg)/" ${SWIGOUTPUTDIR}/gdalconst_wrap.c > ${SWIGOUTPUTDIR}/gdalconst_wrap.c.tmp
	mv -f ${SWIGOUTPUTDIR}/gdalconst_wrap.c.tmp ${SWIGOUTPUTDIR}/gdalconst_wrap.c
	sed "s/PyErr_Format(PyExc_RuntimeError, mesg)/PyErr_SetString(PyExc_RuntimeError, mesg)/" ${SWIGOUTPUTDIR}/ogr_wrap.cpp > ${SWIGOUTPUTDIR}/ogr_wrap.cpp.tmp
	mv -f ${SWIGOUTPUTDIR}/ogr_wrap.cpp.tmp ${SWIGOUTPUTDIR}/ogr_wrap.cpp
	sed "s/PyErr_Format(PyExc_RuntimeError, mesg)/PyErr_SetString(PyExc_RuntimeError, mesg)/" ${SWIGOUTPUTDIR}/osr_wrap.cpp > ${SWIGOUTPUTDIR}/osr_wrap.cpp.tmp
	mv -f ${SWIGOUTPUTDIR}/osr_wrap.cpp.tmp ${SWIGOUTPUTDIR}/osr_wrap.cpp

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
