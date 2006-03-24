

include ../../GDALmake.opt

all: build

BINDING = csharp
include ../SWIGmake.base

EXTRA_DIST = Band.cs ColorTable.cs CoordinateTransformation.cs DataSource.cs Dataset.cs Driver.cs Feature.cs FeatureDefn.cs FieldDefn.cs GCP.cs Geometry.cs Layer.cs MajorObject.cs SpatialReference.cs gdal.cs gdalPINVOKE.cs gdalconst.cs gdalconstPINVOKE.cs ogr.cs ogrPINVOKE.cs osr.cs osrPINVOKE.cs

clean:
	-rm -rf build

veryclean: clean
	-rm -f ${WRAPPERS} ${EXTRA_DIST} SWIGTYPE_p_*.cs

generate: ${WRAPPERS}

build: generate
	@echo "What are you doing?  C# modules needs to be built with msvc"

install: generate
	@echo "What are you doing?  C# modules needs to be built with msvc"
