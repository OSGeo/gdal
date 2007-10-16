

include ../../GDALmake.opt

all: build

BINDING = python
include ../SWIGmake.base

PACKAGE_DIR=osgeo
SWIGOUTPUTDIR=extensions/

PY_COMMANDS     =       epsg_tr.py gdalchksum.py gdal2xyz.py gcps2wld.py \
                        gdalimport.py gdal_merge.py pct2rgb.py rgb2pct.py \
                        gcps2vec.py
PY_MODULES      =       ${PACKAGE_DIR}/gdal.py ${PACKAGE_DIR}/ogr.py ${PACKAGE_DIR}/osr.py ${PACKAGE_DIR}/gdalconst.py

clean:
	-rm -rf build

SWIGARGS += -outdir "${PACKAGE_DIR}" 


veryclean: clean
	-rm -f ${WRAPPERS} ${PY_MODULES}
	-rm -f ${PACKAGE_DIR}/*.pyc
	-rm -f *.pyc
	-rm -f ${SWIGOUTPUTDIR}/gdal_wrap.cpp 
	-rm -f ${SWIGOUTPUTDIR}/gdalconst_wrap.c
	-rm -f ${SWIGOUTPUTDIR}/ogr_wrap.cpp
	-rm -f ${SWIGOUTPUTDIR}/osr_wrap.cpp
	-rm -rf dist
	-rm -f *.so
	-rm -rf *.egg-info

gdal_wrap.cpp: ../include/python/gdal_python.i

ogr_wrap.cpp: ../include/python/ogr_python.i

osr_wrap.cpp: ../include/python/osr_python.i

generate: ${WRAPPERS}

build:
	python setup.py build

install:
ifeq ($(PY_HAVE_SETUPTOOLS),1)
#	python setup.py install --root / --single-version-externally-managed
	python setup.py install 
else
	python setup.py install --prefix=$(DESTDIR)$(prefix)
endif

docs:
	python ../include/python/docs/doxy2swig.py ../../ogr/xml/ogrlayer_8cpp.xml ../include/python/docs/ogr_layer_docs.i OGRLayerShadow OGR_L_

	python ../include/python/docs/doxy2swig.py ../../ogr/xml/ogrgeometry_8cpp.xml ../include/python/docs/ogr_geometry_docs.i OGRGeometryShadow OGR_G_

	python ../include/python/docs/doxy2swig.py ../../ogr/xml/ogrdatasource_8cpp.xml ../include/python/docs/ogr_datasource_docs.i OGRDataSourceShadow OGR_DS_


	python ../include/python/docs/doxy2swig.py ../../ogr/xml/ogrsfdriver_8cpp.xml ../include/python/docs/ogr_driver_docs.i OGRDriverShadow OGR_Dr_

	python ../include/python/docs/doxy2swig.py ../../ogr/xml/ogrfeature_8cpp.xml ../include/python/docs/ogr_feature_docs.i OGRFeatureShadow OGR_F_

	python ../include/python/docs/doxy2swig.py ../../ogr/xml/ogrfeaturedefn_8cpp.xml ../include/python/docs/ogr_featuredef_docs.i OGRFeatureDefnShadow OGR_FD_

	python ../include/python/docs/doxy2swig.py ../../ogr/xml/ogrfielddefn_8cpp.xml ../include/python/docs/ogr_fielddef_docs.i OGRFieldDefnShadow OGR_Fld_
