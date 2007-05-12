

include ../../GDALmake.opt

all:

BINDING = python
include ../SWIGmake.base

PY_COMMANDS     =       epsg_tr.py gdalchksum.py gdal2xyz.py gcps2wld.py \
                        gdalimport.py gdal_merge.py pct2rgb.py rgb2pct.py \
                        gcps2vec.py
PY_MODULES      =       gdal.py ogr.py osr.py gdalconst.py

clean:
	-rm -rf build

veryclean: clean
	-rm -f ${WRAPPERS} ${PY_MODULES}
	-rm -f *.pyc
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
	python setup.py install --prefix=$(DESTDIR)$(prefix)

docs:
	python ../include/python/docs/doxy2swig.py ../../ogr/xml/ogrlayer_8cpp.xml ../include/python/docs/ogr_layer_docs.i OGRLayerShadow OGR_L_

	python ../include/python/docs/doxy2swig.py ../../ogr/xml/ogrgeometry_8cpp.xml ../include/python/docs/ogr_geometry_docs.i OGRGeometryShadow OGR_G_

	python ../include/python/docs/doxy2swig.py ../../ogr/xml/ogrdatasource_8cpp.xml ../include/python/docs/ogr_datasource_docs.i OGRDataSourceShadow OGR_DS_


	python ../include/python/docs/doxy2swig.py ../../ogr/xml/ogrsfdriver_8cpp.xml ../include/python/docs/ogr_driver_docs.i OGRDriverShadow OGR_Dr_

	python ../include/python/docs/doxy2swig.py ../../ogr/xml/ogrfeature_8cpp.xml ../include/python/docs/ogr_feature_docs.i OGRFeatureShadow OGR_F_

	python ../include/python/docs/doxy2swig.py ../../ogr/xml/ogrfeaturedefn_8cpp.xml ../include/python/docs/ogr_featuredef_docs.i OGRFeatureDefnShadow OGR_FD_

	python ../include/python/docs/doxy2swig.py ../../ogr/xml/ogrfielddefn_8cpp.xml ../include/python/docs/ogr_fielddef_docs.i OGRFieldDefnShadow OGR_Fld_
