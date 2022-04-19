# Man pages are not not present in the git repository. They are generated
# at release preparation time by the mkgdaldist.sh script

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/man1/gdalinfo.1)
    install(DIRECTORY man1/ DESTINATION ${CMAKE_INSTALL_MANDIR}/man1
            FILES_MATCHING PATTERN "*.1")
endif()

