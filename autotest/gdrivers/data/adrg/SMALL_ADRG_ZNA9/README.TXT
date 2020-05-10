Copied from data/SMALL_ADRG and modified with the following procedure:
- ../../gdal/frmts/iso8211/8211dump -xml data/SMALL_ADRG_ZNA9/ABCDEF01.GEN > /tmp/tmp.xml
- edit /tmp/tmp.xml to modify the ZNA, ARV, BRV, LSO, PSO to match the ones of the
  dump of ASRP/TEST01/TEST0101.GEN from https://trac.osgeo.org/gdal/attachment/ticket/6560/ASRP_Polar_Test_Data.zip
  Caution in ASRP, LSO and PSO are stored in arc-seconds, whereas in ADRG they are in DD[D]MMSS.SS
- ../../gdal/frmts/iso8211/8211createfromxml /tmp/tmp.xml data/SMALL_ADRG_ZNA9/ABCDEF01.GEN
