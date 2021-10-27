include(GdalStandardIncludes)
# function for csharp build
function (gdal_csharp_dll)
  set(_options)
  set(_oneValueArgs WRAPPER WORKING_DIRECTORY SWIG_INTERFACE NAMESPACE TARGET DEPENDS)
  set(_multiValueArgs CS_SOURCES)
  cmake_parse_arguments(_CSHARP "${_options}" "${_oneValueArgs}" "${_multiValueArgs}" ${ARGN})
  file(MAKE_DIRECTORY ${_CSHARP_WORKING_DIRECTORY})
  add_custom_command(
    OUTPUT ${_CSHARP_WORKING_DIRECTORY}/${_CSHARP_WRAPPER}.cpp ${_CSHARP_CS_SOURCES}
    COMMAND
      ${SWIG_EXECUTABLE} -namespace ${_CSHARP_NAMESPACE} -outdir ${_CSHARP_WORKING_DIRECTORY} -DSWIG2_CSHARP -dllimport
      ${_CSHARP_WRAPPER} -Wall -I${PROJECT_SOURCE_DIR}/swig/include -I${PROJECT_SOURCE_DIR}/swig/include/csharp
      -I${PROJECT_SOURCE_DIR}/gdal -c++ -csharp -o ${_CSHARP_WRAPPER}.cpp ${_CSHARP_SWIG_INTERFACE}
    WORKING_DIRECTORY ${_CSHARP_WORKING_DIRECTORY}
    DEPENDS ${GDAL_SWIG_COMMON_INCLUDE} ${PROJECT_SOURCE_DIR}/swig/include/csharp/typemaps_csharp.i
            ${_CSHARP_SWIG_INTERFACE})
  set_source_files_properties(${_CSHARP_WRAPPER} PROPERTIES GENERATED 1)
  add_library(${_CSHARP_WRAPPER} OBJECT ${_CSHARP_WORKING_DIRECTORY}/${_CSHARP_WRAPPER}.cpp)
  gdal_standard_includes(${_CSHARP_WRAPPER})
  if (_CSHARP_DEPENDS)
    set(CSC_OPTIONS /debug:full /target:library /out:${_CSHARP_TARGET} /r:${_CSHARP_DEPENDS})
  else ()
    set(CSC_OPTIONS /debug:full /target:library /out:${_CSHARP_TARGET})
  endif ()
  add_custom_command(
    OUTPUT ${_CSHARP_TARGET}
    COMMAND ${CSHARP_COMPILER} ${CSC_OPTIONS} ${_CSHARP_CS_SOURCES} ${CMAKE_CURRENT_SOURCE_DIR}/AssemblyInfo.cs
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    DEPENDS ${_CSHARP_WRAPPER} ${_CSHARP_DEPENDS} ${CMAKE_CURRENT_BINARY_DIR}/gdal.snk AssemblyInfo.cs
            ${_CSHARP_CS_SOURCES})
endfunction ()

# ######################################################################################################################
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/gdal.snk
                   COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/gdal.snk ${CMAKE_CURRENT_BINARY_DIR})
gdal_csharp_dll(
  TARGET gdal_csharp.dll
  NAMESPACE OSGeo.GDAL
  WRAPPER gdal_wrap
  SWIG_INTERFACE ${PROJECT_SOURCE_DIR}/swig/include/gdal.i
  CS_SOURCES gdal/Access.cs
             gdal/GDALWarpAppOptions.cs
             gdal/AsyncReader.cs
             gdal/Gdal.cs
             gdal/AsyncStatusType.cs
             gdal/GdalPINVOKE.cs
             gdal/Band.cs
             gdal/MajorObject.cs
             gdal/CPLErr.cs
             gdal/PaletteInterp.cs
             gdal/ColorEntry.cs
             gdal/RATFieldType.cs
             gdal/ColorInterp.cs
             gdal/RATFieldUsage.cs
             gdal/RATTableType.cs
             gdal/ColorTable.cs
             gdal/RIOResampleAlg.cs
             gdal/DataType.cs
             gdal/RWFlag.cs
             gdal/Dataset.cs
             gdal/RasterAttributeTable.cs
             gdal/Driver.cs
             gdal/RasterIOExtraArg.cs
             gdal/GCP.cs
             gdal/ResampleAlg.cs
             gdal/GDALBuildVRTOptions.cs
             gdal/SWIGTYPE_p_GDALProgressFunc.cs
             gdal/GDALDEMProcessingOptions.cs
             gdal/SWIGTYPE_p_int.cs
             gdal/GDALGridOptions.cs
             gdal/SWIGTYPE_p_p_GDALDatasetShadow.cs
             gdal/GDALInfoOptions.cs
             gdal/SWIGTYPE_p_p_GDALRasterBandShadow.cs
             gdal/GDALNearblackOptions.cs
             gdal/Transformer.cs
             gdal/GDALRasterizeOptions.cs
             gdal/XMLNode.cs
             gdal/GDALTranslateOptions.cs
             gdal/XMLNodeType.cs
             gdal/GDALVectorTranslateOptions.cs
             gdal/SWIGTYPE_p_void.cs
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/gdal
  DEPENDS ogr_csharp.dll)
gdal_csharp_dll(
  TARGET ogr_csharp.dll
  NAMESPACE OSGeo.OGR
  WRAPPER ogr_wrap
  SWIG_INTERFACE ${PROJECT_SOURCE_DIR}/swig/include/ogr.i
  CS_SOURCES ogr/DataSource.cs
             ogr/FieldDefn.cs
             ogr/Layer.cs
             ogr/Driver.cs
             ogr/FieldSubType.cs
             ogr/Ogr.cs
             ogr/Envelope.cs
             ogr/FieldType.cs
             ogr/OgrPINVOKE.cs
             ogr/Envelope3D.cs
             ogr/GeomFieldDefn.cs
             ogr/StyleTable.cs
             ogr/Feature.cs
             ogr/Geometry.cs
             ogr/wkbByteOrder.cs
             ogr/FeatureDefn.cs
             ogr/Justification.cs
             ogr/wkbGeometryType.cs
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/ogr
  DEPENDS osr_csharp.dll)
gdal_csharp_dll(
  TARGET osr_csharp.dll
  NAMESPACE OSGeo.OSR
  WRAPPER osr_wrap
  SWIG_INTERFACE ${PROJECT_SOURCE_DIR}/swig/include/osr.i
  CS_SOURCES osr/AxisOrientation.cs osr/Osr.cs osr/SpatialReference.cs osr/CoordinateTransformation.cs osr/OsrPINVOKE.cs
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/osr)
gdal_csharp_dll(
  TARGET gdalconst_csharp.dll
  NAMESPACE OSGeo.GDAL
  WRAPPER gdalconst_wrap
  SWIG_INTERFACE ${PROJECT_SOURCE_DIR}/swig/include/gdalconst.i
  CS_SOURCES const/GdalConst.cs const/GdalConstPINVOKE.cs
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/const)
# ######################################################################################################################
# sample commands
add_custom_command(
  OUTPUT ogrinfo.exe
  COMMAND ${CSHARP_COMPILER} /r:ogr_csharp.dll /r:osr_csharp.dll /out:ogrinfo.exe
          ${CMAKE_CURRENT_SOURCE_DIR}/apps/ogrinfo.cs
  DEPENDS ogr_csharp.dll osr_csharp.dll)
add_custom_command(
  OUTPUT createdata.exe
  COMMAND ${CSHARP_COMPILER} /r:ogr_csharp.dll /r:osr_csharp.dll /out:createdata.exe
          ${CMAKE_CURRENT_SOURCE_DIR}/apps/createdata.cs
  DEPENDS ogr_csharp.dll osr_csharp.dll)
add_custom_command(
  OUTPUT OSRTransform.exe
  COMMAND ${CSHARP_COMPILER} /r:osr_csharp.dll /out:OSRTransform.exe ${CMAKE_CURRENT_SOURCE_DIR}/apps/OSRTransform.cs
  DEPENDS osr_csharp.dll)
add_custom_command(
  OUTPUT GDALRead.exe
  COMMAND ${CSHARP_COMPILER} /r:gdal_csharp.dll /r:System.Drawing.dll /out:GDALRead.exe
          ${CMAKE_CURRENT_SOURCE_DIR}/apps/GDALRead.cs
  DEPENDS gdal_csharp.dll)
add_custom_command(
  OUTPUT GDALReadDirect.exe
  COMMAND ${CSHARP_COMPILER} /r:gdal_csharp.dll /r:System.Drawing.dll /out:GDALReadDirect.exe
          ${CMAKE_CURRENT_SOURCE_DIR}/apps/GDALReadDirect.cs
  DEPENDS gdal_csharp.dll)
add_custom_command(
  OUTPUT GDALAdjustContrast.exe
  COMMAND ${CSHARP_COMPILER} /r:gdal_csharp.dll /r:System.Drawing.dll /out:GDALAdjustContrast.exe
          ${CMAKE_CURRENT_SOURCE_DIR}/apps/GDALAdjustContrast.cs
  DEPENDS gdal_csharp.dll)
add_custom_command(
  OUTPUT GDALDatasetRasterIO.exe
  COMMAND ${CSHARP_COMPILER} /r:gdal_csharp.dll /r:System.Drawing.dll /out:GDALDatasetRasterIO.exe
          ${CMAKE_CURRENT_SOURCE_DIR}/apps/GDALDatasetRasterIO.cs
  DEPENDS gdal_csharp.dll)
add_custom_command(
  OUTPUT GDALWrite.exe
  COMMAND ${CSHARP_COMPILER} /r:gdal_csharp.dll /out:GDALWrite.exe ${CMAKE_CURRENT_SOURCE_DIR}/apps/GDALWrite.cs
  DEPENDS gdal_csharp.dll)
add_custom_command(
  OUTPUT GDALDatasetWrite.exe
  COMMAND ${CSHARP_COMPILER} /r:gdal_csharp.dll /out:GDALDatasetWrite.exe
          ${CMAKE_CURRENT_SOURCE_DIR}/apps/GDALDatasetWrite.cs
  DEPENDS gdal_csharp.dll)
add_custom_command(
  OUTPUT GDALColorTable.exe
  COMMAND ${CSHARP_COMPILER} /r:gdal_csharp.dll /out:GDALColorTable.exe
          ${CMAKE_CURRENT_SOURCE_DIR}/apps/GDALColorTable.cs
  DEPENDS gdal_csharp.dll)
add_custom_command(
  OUTPUT WKT2WKB.exe
  COMMAND ${CSHARP_COMPILER} /r:ogr_csharp.dll /out:WKT2WKB.exe ${CMAKE_CURRENT_SOURCE_DIR}/apps/WKT2WKB.cs
  DEPENDS ogr_csharp.dll)
add_custom_command(
  OUTPUT OGRGEOS.exe
  COMMAND ${CSHARP_COMPILER} /r:ogr_csharp.dll /out:OGRGEOS.exe ${CMAKE_CURRENT_SOURCE_DIR}/apps/OGRGEOS.cs
  DEPENDS ogr_csharp.dll)
add_custom_command(
  OUTPUT ReadXML.exe
  COMMAND ${CSHARP_COMPILER} /r:gdal_csharp.dll /out:ReadXML.exe ${CMAKE_CURRENT_SOURCE_DIR}/apps/ReadXML.cs
  DEPENDS gdal_csharp.dll)
add_custom_command(
  OUTPUT GDALInfo.exe
  COMMAND ${CSHARP_COMPILER} /r:gdal_csharp.dll /r:osr_csharp.dll /out:GDALInfo.exe
          ${CMAKE_CURRENT_SOURCE_DIR}/apps/GDALInfo.cs
  DEPENDS gdal_csharp.dll)
add_custom_command(
  OUTPUT GDALOverviews.exe
  COMMAND ${CSHARP_COMPILER} /r:gdal_csharp.dll /out:GDALOverviews.exe ${CMAKE_CURRENT_SOURCE_DIR}/apps/GDALOverviews.cs
  DEPENDS gdal_csharp.dll)
add_custom_command(
  OUTPUT GDALCreateCopy.exe
  COMMAND ${CSHARP_COMPILER} /r:gdal_csharp.dll /out:GDALCreateCopy.exe
          ${CMAKE_CURRENT_SOURCE_DIR}/apps/GDALCreateCopy.cs
  DEPENDS gdal_csharp.dll)
add_custom_command(
  OUTPUT GDALGetHistogram.exe
  COMMAND ${CSHARP_COMPILER} /r:gdal_csharp.dll /out:GDALGetHistogram.exe
          ${CMAKE_CURRENT_SOURCE_DIR}/apps/GDALGetHistogram.cs
  DEPENDS gdal_csharp.dll)
add_custom_target(
  csharp_samples
  DEPENDS ogrinfo.exe
          createdata.exe
          OSRTransform.exe
          GDALRead.exe
          GDALReadDirect.exe
          GDALAdjustContrast.exe
          GDALDatasetRasterIO.exe
          GDALWrite.exe
          GDALDatasetWrite.exe
          GDALColorTable.exe
          WKT2WKB.exe
          OGRGEOS.exe
          ReadXML.exe
          GDALInfo.exe
          GDALOverviews.exe
          GDALCreateCopy.exe
          GDALGetHistogram.exe)
add_custom_target(csharp_binding ALL DEPENDS csharp_samples gdalconst_csharp.dll gdal_csharp.dll ogr_csharp.dll
                                             osr_csharp.dll)
