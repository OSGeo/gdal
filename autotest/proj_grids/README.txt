* ``conus`` is the "official" NADCON4 conus NAD27->NAD84 file in CTable2 format
* ``egm96_15_extract.gtx`` is an extract generated with ``gdal_translate /usr/share/proj/egm96_15.gtx ../autotest/proj_grids/egm96_15_extract.gtx -srcwin 234 233 3 3``
* ``ca_nrc_NAD83v70VG.tif`` is an extract generated with ``gdal_translate ~/proj/PROJ-data/ca_nrc/ca_nrc_NAD83v70VG.tif ca_nrc_NAD83v70VG.tif -b 1 -b 2 -b 3 -co compress=deflate  -projwin -80 61 -79 60``
