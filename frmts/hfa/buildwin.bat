if exist port\cpl_config.h.vc del port\cpl_config.h
if exist port\cpl_config.h.vc move port\cpl_config.h.vc port\cpl_config.h

cl /c /Ihfa /Ilibtiff /Ilibgeotiff /Iport imggeotiff.c
cl /c /Ihfa /Ilibtiff /Ilibgeotiff /Iport hfa/hfaband.cpp
cl /c /Ihfa /Ilibtiff /Ilibgeotiff /Iport hfa/hfadictionary.cpp
cl /c /Ihfa /Ilibtiff /Ilibgeotiff /Iport hfa/hfaentry.cpp
cl /c /Ihfa /Ilibtiff /Ilibgeotiff /Iport hfa/hfafield.cpp
cl /c /Ihfa /Ilibtiff /Ilibgeotiff /Iport hfa/hfaopen.cpp
cl /c /Ihfa /Ilibtiff /Ilibgeotiff /Iport hfa/hfatype.cpp
cl /c /Ihfa /Ilibtiff /Ilibgeotiff /Iport rawblockedimage.cpp
cl /c /Ihfa /Ilibtiff /Ilibgeotiff /Iport tif_overview.cpp
cl /c /Ihfa /Ilibtiff /Ilibgeotiff /Iport port/cpl_conv.cpp
cl /c /Ihfa /Ilibtiff /Ilibgeotiff /Iport port/cpl_error.cpp
cl /c /Ihfa /Ilibtiff /Ilibgeotiff /Iport port/cpl_string.cpp
cl /c /Ihfa /Ilibtiff /Ilibgeotiff /Iport port/cpl_vsisimple.cpp
cl /c /Ihfa /Ilibtiff /Ilibgeotiff /Iport port/cpl_vsil_win32.cpp
cl /c /Ihfa /Ilibtiff /Ilibgeotiff /Iport port/cpl_path.cpp
cl /c /Ihfa /Ilibtiff /Ilibgeotiff /Iport libgeotiff/geo_extra.c
cl /c /Ihfa /Ilibtiff /Ilibgeotiff /Iport libgeotiff/geo_free.c
cl /c /Ihfa /Ilibtiff /Ilibgeotiff /Iport libgeotiff/geo_get.c
cl /c /Ihfa /Ilibtiff /Ilibgeotiff /Iport libgeotiff/geo_names.c
cl /c /Ihfa /Ilibtiff /Ilibgeotiff /Iport libgeotiff/geo_new.c
cl /c /Ihfa /Ilibtiff /Ilibgeotiff /Iport libgeotiff/geo_print.c
cl /c /Ihfa /Ilibtiff /Ilibgeotiff /Iport libgeotiff/geo_set.c
cl /c /Ihfa /Ilibtiff /Ilibgeotiff /Iport libgeotiff/geo_tiffp.c
cl /c /Ihfa /Ilibtiff /Ilibgeotiff /Iport libgeotiff/geo_write.c
cl /c /Ihfa /Ilibtiff /Ilibgeotiff /Iport libgeotiff/xtiff.c
cl /c /Ihfa /Ilibtiff /Ilibgeotiff /Iport libtiff/tif_aux.c
cl /c /Ihfa /Ilibtiff /Ilibgeotiff /Iport libtiff/tif_close.c
cl /c /Ihfa /Ilibtiff /Ilibgeotiff /Iport libtiff/tif_codec.c
cl /c /Ihfa /Ilibtiff /Ilibgeotiff /Iport libtiff/tif_compress.c
cl /c /Ihfa /Ilibtiff /Ilibgeotiff /Iport libtiff/tif_dir.c
cl /c /Ihfa /Ilibtiff /Ilibgeotiff /Iport libtiff/tif_dirinfo.c
cl /c /Ihfa /Ilibtiff /Ilibgeotiff /Iport libtiff/tif_dirread.c
cl /c /Ihfa /Ilibtiff /Ilibgeotiff /Iport libtiff/tif_dirwrite.c
cl /c /Ihfa /Ilibtiff /Ilibgeotiff /Iport libtiff/tif_dumpmode.c
cl /c /Ihfa /Ilibtiff /Ilibgeotiff /Iport libtiff/tif_error.c
cl /c /Ihfa /Ilibtiff /Ilibgeotiff /Iport libtiff/tif_fax3.c
cl /c /Ihfa /Ilibtiff /Ilibgeotiff /Iport libtiff/tif_fax3sm.c
cl /c /Ihfa /Ilibtiff /Ilibgeotiff /Iport libtiff/tif_flush.c
cl /c /Ihfa /Ilibtiff /Ilibgeotiff /Iport libtiff/tif_getimage.c
cl /c /Ihfa /Ilibtiff /Ilibgeotiff /Iport libtiff/tif_jpeg.c
cl /c /Ihfa /Ilibtiff /Ilibgeotiff /Iport libtiff/tif_luv.c
cl /c /Ihfa /Ilibtiff /Ilibgeotiff /Iport libtiff/tif_lzw.c
cl /c /Ihfa /Ilibtiff /Ilibgeotiff /Iport libtiff/tif_next.c
cl /c /Ihfa /Ilibtiff /Ilibgeotiff /Iport libtiff/tif_open.c
cl /c /Ihfa /Ilibtiff /Ilibgeotiff /Iport libtiff/tif_packbits.c
cl /c /Ihfa /Ilibtiff /Ilibgeotiff /Iport libtiff/tif_pixarlog.c
cl /c /Ihfa /Ilibtiff /Ilibgeotiff /Iport libtiff/tif_predict.c
cl /c /Ihfa /Ilibtiff /Ilibgeotiff /Iport libtiff/tif_print.c
cl /c /Ihfa /Ilibtiff /Ilibgeotiff /Iport libtiff/tif_read.c
cl /c /Ihfa /Ilibtiff /Ilibgeotiff /Iport libtiff/tif_strip.c
cl /c /Ihfa /Ilibtiff /Ilibgeotiff /Iport libtiff/tif_swab.c
cl /c /Ihfa /Ilibtiff /Ilibgeotiff /Iport libtiff/tif_thunder.c
cl /c /Ihfa /Ilibtiff /Ilibgeotiff /Iport libtiff/tif_tile.c
cl /c /Ihfa /Ilibtiff /Ilibgeotiff /Iport libtiff/tif_vsi.c
cl /c /Ihfa /Ilibtiff /Ilibgeotiff /Iport libtiff/tif_version.c
cl /c /Ihfa /Ilibtiff /Ilibgeotiff /Iport libtiff/tif_warning.c
cl /c /Ihfa /Ilibtiff /Ilibgeotiff /Iport libtiff/tif_write.c
cl /c /Ihfa /Ilibtiff /Ilibgeotiff /Iport libtiff/tif_zip.c

cl /c /Ihfa /Ilibtiff /Ilibgeotiff /Iport img2tif.cpp
cl *.obj /Feimg2tif.exe

del img2tif.obj
cl /c /Ihfa /Ilibtiff /Ilibgeotiff /Iport hfatest.cpp
cl *.obj /Fehfatest.exe
