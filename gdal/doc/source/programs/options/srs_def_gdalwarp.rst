The coordinate systems that can be passed are anything supported by the
OGRSpatialReference.SetFromUserInput() call, which includes EPSG PCS and GCSes
(i.e. EPSG:4296), PROJ.4 declarations (as above), or the name of a .prj file
containing well known text. Starting with GDAL 2.2, if the SRS has an explicit
vertical datum that points to a PROJ.4 geoidgrids, and the input dataset is a
single band dataset, a vertical correction will be applied to the values of the
dataset.
