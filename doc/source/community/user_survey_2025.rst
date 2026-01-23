.. _survey_2025:

2025 GDAL user survey
=====================

.. only:: not html

   Results of GDAL user surveys are available at https://gdal.org.

.. only:: html


    In December 2025, The GDAL maintenance program conducted its second open survey to collect feedback on user’s experience with GDAL and the direction of the maintenance program. The survey was publicized on gdal.org, the gdal-dev mailing list, the project GitHub page, and social media. From December 01 to January 12, the survey received 592 responses.

    Responses to the survey were broadly consistent with 2024. This page presents the results of key survey questions with a comparison to the 2024 survey where applicable.

   Who responded to the survey?
   ----------------------------
   
   As in 2024, survey respondents were generally very experienced users, with 78% of users having spent 5 or more years working with GDAL. Nearly half (49%) have built GDAL from source, 28% have contributed to the project by submitting bug reports or pull requests, and 8% participate in the gdal-dev mailing list. The high experience level of respondents reflects the challenge of reaching users who may use GDAL less often, through other software, or are not connected to the project community via mailing lists or social media.

   .. image:: ../../images/community/survey_2025/years_experience.svg
   
   Respondents get news about GDAL (presumably including the availability of the survey itself) from a variety of sources.

   .. image:: ../../images/community/survey_2025/news_source.svg

   How is GDAL used?
   -----------------

   GDAL encompasses many different capabilities, with support for more than 200 data formats, a large command line interface, and bindings to multiple programming languages.

   Several questions were included in the survey to learn more about which parts of GDAL are most used.

   Raster data formats
   ^^^^^^^^^^^^^^^^^^^

   Among raster data formats, GeoTIFF continues to command an overwhelming majority of GDAL usage:

   .. image:: ../../images/community/survey_2025/raster_data_formats.svg

   Although difficult to see given the scale of the plot above, reported Zarr usage increased significantly between 2024 and 2025. More than a third of users expect to be using this format in the future, even if they are not using it currently.
   
   .. image:: ../../images/community/survey_2025/raster_data_formats_future.svg
   
   Vector data formats
   ^^^^^^^^^^^^^^^^^^^
   
   The most popular vector formats remain GeoPackage, Shapefile, GeoJSON and PostGIS. The responses clearly show increasing adoption of GeoParquet.

   .. image:: ../../images/community/survey_2025/vector_data_formats.svg

   More than half of respondents anticipate using GeoParquet in the future:

   .. image:: ../../images/community/survey_2025/vector_data_formats_future.svg

   Local or cloud?
   ^^^^^^^^^^^^^^^^

   .. image:: ../../images/community/survey_2025/local_or_cloud_1.svg

   .. image:: ../../images/community/survey_2025/local_or_cloud_2.svg
   
   Usage environments
   ------------------

   Python remains the most common way for users to work with GDAL, with an approximately 50/50 split between the GDAL Python bindings and higher-level packages such as shapely, rasterio, and geopandas. Approximately a third of GDAL users work primarily with the “classic” command-line interface, followed by smaller numbers of using R and QGIS.

   Although 41% of respondents reported using the new command line interface (introduced in the May 2025 release), fewer than 10% report that it is the most common way they use GDAL.
   
   .. image:: ../../images/community/survey_2025/way_gdal_used.svg

   Focus areas for GDAL development
   --------------------------------

   Several questions asked users for information about their difficulties working
   with GDAL and ideas for future improvements to the software.

   Difficulties using GDAL
   ^^^^^^^^^^^^^^^^^^^^^^^

   Users did not identify a single area as a source of their challenges with GDAL. The top response of “finding examples” indicates a continued shortage of documentation. Installation remains a persistent challenge for some users.

   .. image:: ../../images/community/survey_2025/gdal_challenge.svg
   
   Similarly “examples,” “documentation,” and “install” are often mentioned in the open-ended responses to “what could make GDAL easier to use?”
   
   .. image:: ../../images/community/survey_2025/gdal_easier_to_use.svg

   Installation difficulties
   ^^^^^^^^^^^^^^^^^^^^^^^^^
   
   Installation difficulties are not limited to a single platform, with a contingent of users across multiple platforms reporting challenges. Several responses to the “general feedback” flagged Python installation as an area of difficulty, especially compared to third-party Python packages that use GDAL such as ``fiona`` and ``rasterio``.
   
   .. image:: ../../images/community/survey_2025/easy_to_install_gdal.svg

   Documentation needs
   ^^^^^^^^^^^^^^^^^^^

   Respondents reported “examples”, “workflows”, and “API usage” as high priorities for documentation efforts.
   
   .. image:: ../../images/community/survey_2025/documentation_needs.svg

   Sponsorship program activities
   ------------------------------
   
   Among activities undertaken by the sponsorship program so far, respondents found the most value in enhancements to GDAL’s dependencies (such as PROJ, GEOS, and libtiff), its Python bindings, and documentation.

   About a third of users reported that the new command-line interface already improved their usability of GDAL.

   .. image:: ../../images/community/survey_2025/maintenance_program_activities.svg

   Respondents suggested similar areas of maintenance program focus in 2024 and 2025, with high value given to performance improvements, format compatibility, integration with other software, and continued (backwards-compatible!) command-line development.

   .. image:: ../../images/community/survey_2025/maintenance_program_areas_of_focus.svg
   
   Security
   --------

   Some questions about security issues were added to the 2025 survey.

   Most users assess GDAL to be secure software, although 5% report that their usage of GDAL has been affected by security vulnerabilities.

   .. image:: ../../images/community/survey_2025/is_gdal_secure.svg
   
   LLM usage by GDAL developers and users
   --------------------------------------

   Most users (67%) consider it appropriate for GDAL code to be developed using LLMs. A smaller fraction (40%) report that LLMs have been helpful in their own usage of GDAL.

   The areas where users support GDAL’s LLM usage vary, with usage in code review and test development supported by a majority of users.
   
   .. image:: ../../images/community/survey_2025/llm_use_areas.svg
