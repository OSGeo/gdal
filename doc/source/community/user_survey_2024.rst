.. _survey_2024:

2024 GDAL user survey
=====================

.. only:: not html

   Results of the 2024 GDAL user survey are available at https://gdal.org.

.. only:: html

   In October 2024, The GDAL maintenance program created an open survey to collect
   feedback on user's experience with GDAL and the direction of the maintenance
   program. The survey was publicized on gdal.org, the gdal-dev mailing list, the
   project GitHub page, and social media. From October 28 to November 21, the
   survey received 602 responses.
   
   Who responded to the survey?
   ----------------------------
   
   Survey respondents were generally very experienced users, with 79% of users
   having spent 5 or more years working with GDAL. Surprisingly, two respondents
   claimed to be Frank Warmerdam, who originated the project in 1998. More than
   half (52%) have built GDAL from source, 29% subscribe to the gdal-dev mailing
   list, and 29% have contributed to the project by submitting bug reports or pull
   requests. The high experience level of respondents reflects the challenge of
   reaching users who may use GDAL less often, through other software, or are not
   connected to the project community via mailing lists or social media.
   
   .. image:: ../../images/community/survey_2024/years_experience.svg
   
   Operating system
   ^^^^^^^^^^^^^^^^
   
   Most survey respondents use GDAL on Linux, followed by Windows and OS X.
   Responses to "Other" included WSL2 and iOS.
   
   .. image:: ../../images/community/survey_2024/operating_system.svg
   
   Local or cloud?
   ^^^^^^^^^^^^^^^
   
   Most survey respondents use GDAL primarily with local file systems.
   
   .. image:: ../../images/community/survey_2024/local_or_cloud_read.svg
   
   .. image:: ../../images/community/survey_2024/local_or_cloud_write.svg
   
   Data formats
   ^^^^^^^^^^^^
   
   Among raster data formats, GeoTIFF commands an overwhelming majority of GDAL
   usage:
   
   .. image:: ../../images/community/survey_2024/raster_data_formats.svg
   
   The most popular vector format was GeoPackage, followed by classics such as
   Shapefile, GeoJSON, and PostGIS. GeoParquet, Esri FileGeodatabase, FlatGeobuf,
   and GML each earned enough votes to remain out of the "Other" category.
   
   .. image:: ../../images/community/survey_2024/vector_data_formats.svg
   
   Installing GDAL
   ---------------
   
   Survey respondents obtain GDAL from a variety of channels, depending on the
   platform.  On Linux, standard system packages are the most popular solution.
   OSX users rely primarily on Homebrew; most Windows users user OSGeo4W. The
   popularity of Homebrew among Windows users may indicate that GDAL is being used
   through the Windows Subsystem for Linux (WSL). The reported usage of OSGeo4W by
   Linux users is more difficult to explain.
   
   .. image:: ../../images/community/survey_2024/where_gdal_obtained_linux.svg
   
   .. image:: ../../images/community/survey_2024/where_gdal_obtained_osx.svg
   
   .. image:: ../../images/community/survey_2024/where_gdal_obtained_windows.svg
   
   As may be expected for a group of experienced users, most respondents reported
   that GDAL is easy to install with the options they need. Still, installation
   remains a difficulty for many users.
   
   .. image:: ../../images/community/survey_2024/easy_to_install_gdal.svg
   
   Installation difficulties were not associated with a particular operating
   system.
   
   .. image:: ../../images/community/survey_2024/easy_to_install_gdal_os.svg
   
   How is GDAL used?
   -----------------
   
   The greatest number of respondents reported using GDAL from Python, with a
   roughly 50/50 split between the GDAL Python bindings and higher-level packages
   such as shapely, rasterio, and geopandas. After Python, the greatest number of
   respondents reported using the command line interface, followed by smaller
   number of users working in R, PostGIS, and QGIS.
   
   .. image:: ../../images/community/survey_2024/way_gdal_used.svg
   
   Getting help with GDAL
   ----------------------
   
   Most users use gdal.org (directly or via a search engine) as their starting
   point when trying to get help with GDAL.
   
   .. image:: ../../images/community/survey_2024/gdal_help_source.svg
   
   Difficulties using GDAL
   -----------------------
   
   Users did not identify a single area as a source of their challenges with GDAL.
   However, the top responses of "finding examples" and "understanding features"
   point to a shortage of documentation.
   
   .. image:: ../../images/community/survey_2024/gdal_challenge.svg
   
   Consistent with the above, respondents reported "examples", "workflows", and
   "API usage" as high priorities for documentation efforts.
   
   .. image:: ../../images/community/survey_2024/documentation_needs.svg
   
   And "examples" and "doc" rank highly among open-ended responses to
   "what could make GDAL easier to use?"
   
   .. image:: ../../images/community/survey_2024/gdal_easier_to_use.svg
   
   Maintenance program activities
   ------------------------------
   
   Among activities undertaken by the maintenance program so far, respondents found
   the most value in enhancements to GDAL's dependencies (such as PROJ, GEOS, and
   libtiff), its Python bindings, and documentation.
   
   .. image:: ../../images/community/survey_2024/maintenance_program_activities.svg
   
   Asked about a variety of tasks the maintenance program could take on beyond
   those listed above, respondents showed some enthusiasm for almost everything!
   Still, high priorities were given to performance, improving format
   capabilities, and improving the command line interface while preserving
   backward compatibility.
   
   .. image:: ../../images/community/survey_2024/maintenance_program_areas_of_focus.svg
   
   Next steps
   ----------
   
   The maintenance program will use these results to inform work over the coming
   year.  Some work has already been performed to `develop an improved
   command-line interface <https://github.com/OSGeo/gdal/pull/11303>`__ and `add a
   mechanism for usage examples to be cross-referenced in the
   documentation <https://github.com/OSGeo/gdal/pull/11271>`__.
