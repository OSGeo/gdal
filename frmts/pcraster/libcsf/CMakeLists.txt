add_library(
  csf OBJECT
  _getcell.c
  cellsize.c
  delattr.c
  gdattype.c
  gnrrows.c
  legend.c
  pminval.c
  rcomp.c
  setangle.c
  vsdef.c
  _getrow.c
  create2.c
  dumconv.c
  getattr.c
  gproj.c
  mclose.c
  putallmv.c
  rcoords.c
  setmv.c
  vsis.c
  _gsomece.c
  csf.h
  endian.c
  getx0.c
  gputproj.c
  mopen.c
  putattr.c
  rdup2.c
  setvtmv.c
  vsvers.c
  _putcell.c
  csfattr.h
  file.c
  gety0.c
  gvalscal.c
  moreattr.c
  putsomec.c
  reseterr.c
  strconst.c
  wattrblk.c
  _rputrow.c
  csfglob.c
  filename.c
  ggisfid.c
  gvartype.c
  mperror.c
  putx0.c
  rextend.c
  strpad.c
  angle.c
  csfimpl.h
  gattrblk.c
  gmaxval.c
  gversion.c
  pcrtypes.h
  puty0.c
  rmalloc.c
  swapio.c
  attravai.c
  csfsup.c
  gattridx.c
  gminval.c
  ismv.c
  pgisfid.c
  pvalscal.c
  rrowcol.c
  trackmm.c
  attrsize.c
  csftypes.h
  gcellrep.c
  gnrcols.c
  kernlcsf.c
  pmaxval.c
  rattrblk.c
  ruseas.c
  vs2.c)
target_compile_options(csf PRIVATE ${GDAL_C_WARNING_FLAGS} ${GDAL_SOFTWARNFLAGS})
target_include_directories(csf PRIVATE $<TARGET_PROPERTY:cpl,SOURCE_DIR> $<TARGET_PROPERTY:cpl,BINARY_DIR>)
target_compile_definitions(csf PRIVATE -DUSE_IN_GDAL)
set_property(TARGET csf PROPERTY POSITION_INDEPENDENT_CODE ${GDAL_OBJECT_LIBRARIES_POSITION_INDEPENDENT_CODE})

is_plugin(RES gdal_PCRaster)
if (RES)
  target_sources(gdal_PCRaster PRIVATE $<TARGET_OBJECTS:csf>)
else ()
  target_sources(${GDAL_LIB_TARGET_NAME} PRIVATE $<TARGET_OBJECTS:csf>)
endif ()
