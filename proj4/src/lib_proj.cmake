##############################################
### SWITCH BETWEEN STATIC OR SHARED LIBRARY###
##############################################
colormsg(_HIBLUE_ "Configuring proj library:")
message(STATUS "")

# default config, shared on unix and static on Windows
if(UNIX)
    set(BUILD_LIBPROJ_SHARED_DEFAULT ON )
endif(UNIX)
if( WIN32)
    set(BUILD_LIBPROJ_SHARED_DEFAULT OFF)
endif(WIN32)
option(BUILD_LIBPROJ_SHARED "Build libproj library shared." ${BUILD_LIBPROJ_SHARED_DEFAULT})
if(BUILD_LIBPROJ_SHARED)
  set(PROJ_LIBRARY_TYPE SHARED)
else(BUILD_LIBPROJ_SHARED)
  set(PROJ_LIBRARY_TYPE STATIC)
endif(BUILD_LIBPROJ_SHARED)


option(USE_THREAD "Build libproj with thread/mutex support " ON)
if(NOT USE_THREAD)
   add_definitions( -DMUTEX_stub)
endif(NOT USE_THREAD)
find_package(Threads QUIET)
if(USE_THREAD AND Threads_FOUND AND CMAKE_USE_WIN32_THREADS_INIT )
   add_definitions( -DMUTEX_win32)
endif(USE_THREAD AND Threads_FOUND AND CMAKE_USE_WIN32_THREADS_INIT )
if(USE_THREAD AND Threads_FOUND AND CMAKE_USE_PTHREADS_INIT )
   add_definitions( -DMUTEX_pthread)
endif(USE_THREAD AND Threads_FOUND AND CMAKE_USE_PTHREADS_INIT )
if(USE_THREAD AND NOT Threads_FOUND)
  message(FATAL_ERROR "No thread library found and thread/mutex support is required by USE_THREAD option")
endif(USE_THREAD AND NOT Threads_FOUND)


##############################################
### librairie source list and include_list ###
##############################################
SET(SRC_LIBPROJ_PJ
        nad_init.c
        PJ_aea.c
        PJ_aeqd.c
        PJ_airy.c
        PJ_aitoff.c
        PJ_august.c
        PJ_bacon.c
        PJ_bipc.c
        PJ_boggs.c
        PJ_bonne.c
        PJ_calcofi.c
        PJ_cass.c
        PJ_cc.c
        PJ_cea.c
        PJ_chamb.c
        PJ_collg.c
        PJ_comill.c
        PJ_crast.c
        PJ_denoy.c
        PJ_eck1.c
        PJ_eck2.c
        PJ_eck3.c
        PJ_eck4.c
        PJ_eck5.c
        PJ_eqc.c
        PJ_eqdc.c
        PJ_fahey.c
        PJ_fouc_s.c
        PJ_gall.c
        PJ_geos.c
        PJ_gins8.c
        PJ_gnom.c
        PJ_gn_sinu.c
        PJ_goode.c
        PJ_gstmerc.c
        PJ_hammer.c
        PJ_hatano.c
        PJ_igh.c
        PJ_isea.c
        PJ_imw_p.c
        PJ_krovak.c
        PJ_labrd.c
        PJ_laea.c
        PJ_lagrng.c
        PJ_larr.c
        PJ_lask.c
        PJ_lcca.c
        PJ_lcc.c
        PJ_loxim.c
        PJ_lsat.c
        PJ_misrsom.c
        PJ_mbt_fps.c
        PJ_mbtfpp.c
        PJ_mbtfpq.c
        PJ_merc.c
        PJ_mill.c
        PJ_mod_ster.c
        PJ_moll.c
        PJ_natearth.c
        PJ_natearth2.c
        PJ_nell.c
        PJ_nell_h.c
        PJ_nocol.c
        PJ_nsper.c
        PJ_nzmg.c
        PJ_ob_tran.c
        PJ_ocea.c
        PJ_oea.c
        PJ_omerc.c
        PJ_ortho.c
        PJ_patterson.c
        PJ_poly.c
        PJ_putp2.c
        PJ_putp3.c
        PJ_putp4p.c
        PJ_putp5.c
        PJ_putp6.c
        PJ_qsc.c
        PJ_robin.c
        PJ_rpoly.c
        PJ_sch.c
        PJ_sconics.c
        PJ_somerc.c
        PJ_sterea.c
        PJ_stere.c
        PJ_sts.c
        PJ_tcc.c
        PJ_tcea.c
        PJ_times.c
        PJ_tmerc.c
        PJ_tpeqd.c
        PJ_urm5.c
        PJ_urmfps.c
        PJ_vandg.c
        PJ_vandg2.c
        PJ_vandg4.c
        PJ_wag2.c
        PJ_wag3.c
        PJ_wag7.c
        PJ_wink1.c
        PJ_wink2.c
        proj_etmerc.c
)

SET(SRC_LIBPROJ_CORE
        aasincos.c
        adjlon.c
        bch2bps.c
        bchgen.c
        biveval.c
        dmstor.c
        emess.c
        emess.h
        geocent.c
        geocent.h
        geodesic.c
        mk_cheby.c
        nad_cvt.c
        nad_init.c
        nad_intr.c
        pj_apply_gridshift.c
        pj_apply_vgridshift.c
        pj_auth.c
        pj_ctx.c
        pj_fileapi.c
        pj_datum_set.c
        pj_datums.c
        pj_deriv.c
        pj_ell_set.c
        pj_ellps.c
        pj_errno.c
        pj_factors.c
        pj_fwd.c
        pj_fwd3d.c
        pj_gauss.c
        pj_gc_reader.c
        pj_generic_selftest.c
        pj_geocent.c
        pj_gridcatalog.c
        pj_gridinfo.c
        pj_gridlist.c
        PJ_healpix.c
        pj_init.c
        pj_initcache.c
        pj_inv.c
        pj_inv3d.c
        pj_latlong.c
        pj_list.c
        pj_list.h
        pj_log.c
        pj_malloc.c
        pj_mlfn.c
        pj_msfn.c
        pj_mutex.c
        pj_open_lib.c
        pj_param.c
        pj_phi2.c
        pj_pr_list.c
        pj_qsfn.c
        pj_release.c
        pj_run_selftests.c
        pj_strerrno.c
        pj_transform.c
        pj_tsfn.c
        pj_units.c
        pj_utils.c
        pj_zpoly1.c
        proj_mdist.c
        proj_rouss.c
        rtodms.c
        vector1.c
        pj_strtod.c
        ${CMAKE_CURRENT_BINARY_DIR}/proj_config.h
 )

set(HEADERS_LIBPROJ
        projects.h
        proj_api.h
        geodesic.h
)

# Group source files for IDE source explorers (e.g. Visual Studio)
source_group("Header Files" FILES ${HEADERS_LIBPROJ})
source_group("Source Files\\Core" FILES ${SRC_LIBPROJ_CORE})
source_group("Source Files\\PJ" FILES ${SRC_LIBPROJ_PJ})
include_directories( ${CMAKE_CURRENT_BINARY_DIR})
source_group("CMake Files" FILES CMakeLists.txt)


# Embed PROJ_LIB data files location
add_definitions(-DPROJ_LIB="${CMAKE_INSTALL_PREFIX}/${DATADIR}")

#################################################
## java wrapping with jni
#################################################
option(JNI_SUPPORT "Build support of java/jni wrapping for proj library" OFF)
find_package(JNI QUIET)
if(JNI_SUPPORT AND NOT JNI_FOUND)
  message(FATAL_ERROR "jni support is required but jni is not found")
endif(JNI_SUPPORT AND NOT JNI_FOUND)
boost_report_value(JNI_SUPPORT)
if(JNI_SUPPORT)
  set(SRC_LIBPROJ_CORE ${SRC_LIBPROJ_CORE}
                       jniproj.c )
  set(HEADERS_LIBPROJ ${HEADERS_LIBPROJ}
                        org_proj4_PJ.h
                        org_proj4_Projections.h)
  source_group("Source Files\\JNI" FILES ${SRC_LIBPROJ_JNI})
  add_definitions(-DJNI_ENABLED)
  include_directories( ${JNI_INCLUDE_DIRS})
  boost_report_value(JNI_INCLUDE_DIRS)
endif(JNI_SUPPORT)

#################################################
## targets: libproj and proj_config.h
#################################################
set(ALL_LIBPROJ_SOURCES ${SRC_LIBPROJ_PJ} ${SRC_LIBPROJ_CORE})
set(ALL_LIBPROJ_HEADERS ${HEADERS_LIBPROJ} )
if(WIN32 AND BUILD_LIBPROJ_SHARED)
    set(ALL_LIBPROJ_SOURCES ${ALL_LIBPROJ_SOURCES} proj.def )
endif(WIN32 AND BUILD_LIBPROJ_SHARED)

# Core targets configuration
string(TOLOWER "${PROJECT_INTERN_NAME}" PROJECTNAMEL)
set(PROJ_CORE_TARGET ${PROJECTNAMEL})
proj_target_output_name(${PROJ_CORE_TARGET} PROJ_CORE_TARGET_OUTPUT_NAME)

add_library( ${PROJ_CORE_TARGET}
                    ${PROJ_LIBRARY_TYPE}
                    ${ALL_LIBPROJ_SOURCES}
                    ${ALL_LIBPROJ_HEADERS}
                    ${PROJ_RESOURCES}  )


if(WIN32)
  set_target_properties(${PROJ_CORE_TARGET}
    PROPERTIES
    VERSION "${${PROJECT_INTERN_NAME}_BUILD_VERSION}"
    OUTPUT_NAME "${PROJ_CORE_TARGET_OUTPUT_NAME}"
    CLEAN_DIRECT_OUTPUT 1)
elseif(BUILD_FRAMEWORKS_AND_BUNDLE)
  set_target_properties(${PROJ_CORE_TARGET}
    PROPERTIES
    VERSION "${${PROJECT_INTERN_NAME}_BUILD_VERSION}"
    INSTALL_NAME_DIR ${PROJ_INSTALL_NAME_DIR}
    CLEAN_DIRECT_OUTPUT 1)
else()
  set_target_properties(${PROJ_CORE_TARGET}
    PROPERTIES
    VERSION "${${PROJECT_INTERN_NAME}_BUILD_VERSION}"
    SOVERSION "${${PROJECT_INTERN_NAME}_API_VERSION}"
    CLEAN_DIRECT_OUTPUT 1)
endif()

set_target_properties(${PROJ_CORE_TARGET}
    PROPERTIES
    LINKER_LANGUAGE C)

##############################################
# Link properties
##############################################
set(PROJ_LIBRARIES ${PROJ_CORE_TARGET} )
if(UNIX)
    find_library(M_LIB m)
    if(M_LIB)
      TARGET_LINK_LIBRARIES(${PROJ_CORE_TARGET} -lm)
    endif()
endif(UNIX)
if(USE_THREAD AND Threads_FOUND AND CMAKE_USE_PTHREADS_INIT)
   TARGET_LINK_LIBRARIES(${PROJ_CORE_TARGET} ${CMAKE_THREAD_LIBS_INIT})
endif(USE_THREAD AND Threads_FOUND AND CMAKE_USE_PTHREADS_INIT)


##############################################
# install
##############################################
install(TARGETS ${PROJ_CORE_TARGET}
        EXPORT targets
        RUNTIME DESTINATION ${BINDIR}
        LIBRARY DESTINATION ${LIBDIR}
        ARCHIVE DESTINATION ${LIBDIR}
        FRAMEWORK DESTINATION ${FRAMEWORKDIR})

if(NOT BUILD_FRAMEWORKS_AND_BUNDLE)
  install(FILES ${ALL_LIBPROJ_HEADERS}
        DESTINATION ${INCLUDEDIR})
endif(NOT BUILD_FRAMEWORKS_AND_BUNDLE)

##############################################
# Core configuration summary
##############################################
boost_report_value(PROJ_CORE_TARGET)
boost_report_value(PROJ_CORE_TARGET_OUTPUT_NAME)
boost_report_value(PROJ_LIBRARY_TYPE)
boost_report_value(PROJ_LIBRARIES)




