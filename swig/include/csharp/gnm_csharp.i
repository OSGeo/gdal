/******************************************************************************
 * $Id$
******************************************************************************/

%include cpl_exceptions.i

%rename (RegisterAll) OGRRegisterAll();

%include typemaps_csharp.i

DEFINE_EXTERNAL_CLASS(OSRSpatialReferenceShadow, OSGeo.OSR.SpatialReference)
DEFINE_EXTERNAL_CLASS(OSRCoordinateTransformationShadow, OSGeo.OSR.CoordinateTransformation)
DEFINE_EXTERNAL_CLASS(GDALMajorObjectShadow, OSGeo.GDAL.MajorObject)

