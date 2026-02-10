# Configuration file for the Sphinx documentation builder.
#
# This file only contains a selection of the most common options. For a full
# list see the documentation:
# http://www.sphinx-doc.org/en/master/config

# -- Path setup --------------------------------------------------------------

# If extensions (or modules to document with autodoc) are in another directory,
# add these directories to sys.path here. If the directory is relative to the
# documentation root, use os.path.abspath to make it absolute, like shown here.
#
import datetime
import os
import shutil
import sys

build_dir = os.environ.get("BUILDDIR", "../build")
if build_dir == "build":
    build_dir = "../build"

sys.path.insert(0, os.path.abspath("_extensions"))


def check_python_bindings():
    # -- Check we can load the GDAL Python bindings

    import traceback

    from sphinx.util import logging

    logger = logging.getLogger(__name__)
    try:
        from osgeo import gdal
    except ImportError as e:
        logger.warn(
            "Failed to load GDAL Python bindings. The Python bindings must be accessible to build Python API documentation."
        )
        if sys.version_info < (3, 10):
            exc_info = sys.exc_info()
            for line in traceback.format_exception(*exc_info):
                logger.info(line[:-1])
        else:
            for line in traceback.format_exception(e):
                logger.info(line[:-1])
    else:
        version_file = os.path.join(
            os.path.dirname(__file__), os.pardir, os.pardir, "VERSION"
        )
        doc_version = open(version_file).read().strip()
        doc_version_stripped = doc_version
        for suffix in ["dev", "beta"]:
            pos_suffix = doc_version_stripped.find(suffix)
            if pos_suffix > 0:
                doc_version_stripped = doc_version_stripped[0:pos_suffix]

        gdal_version = gdal.__version__
        gdal_version_stripped = gdal_version
        for suffix in ["dev", "beta"]:
            pos_suffix = gdal_version_stripped.find(suffix)
            if pos_suffix > 0:
                gdal_version_stripped = gdal_version_stripped[0:pos_suffix]

        if doc_version_stripped != gdal_version_stripped:
            logger.warn(
                f"Building documentation for GDAL {doc_version_stripped} but osgeo.gdal module has version {gdal_version_stripped}. Python API documentation may be incorrect."
            )


# -- Project information -----------------------------------------------------

project = "GDAL"
copyright = "1998-" + str(datetime.date.today().year)
author = "Frank Warmerdam, Even Rouault, and others"

# -- General configuration ---------------------------------------------------

# Add any Sphinx extension module names here, as strings. They can be
# extensions coming with Sphinx (named 'sphinx.ext.*') or your custom
# ones.
extensions = [
    "breathe",
    "configoptions",
    "driverproperties",
    "cli_example",
    "doctestplus_gdal",
    "source_file",
    "sphinx.ext.napoleon",
    "sphinxcontrib.cairosvgconverter",
    "sphinxcontrib.jquery",
    "sphinxcontrib_programoutput_gdal",
    "sphinxcontrib.spelling",
    "myst_nb",
    "sphinx_tabs.tabs",
    "sphinx_toolbox.collapse",
]

# Add any paths that contain templates here, relative to this directory.
templates_path = ["_templates"]

# List of patterns, relative to source directory, that match files and
# directories to ignore when looking for source files.
# This pattern also affects html_static_path and html_extra_path.
exclude_patterns = [
    "substitutions.rst",
    "programs/options/*.rst",
    "api/python/modules.rst",
    "gdal_rtd/README.md",
]

# Prevents double hyphen (--) to be replaced by Unicode long dash character
# Cf https://stackoverflow.com/questions/15258831/how-to-handle-two-dashes-in-rest
smartquotes = False

# Read the file of substitutions and append it to the beginning of each file.
# This avoids the need to add an explicit ..include directive to every file.
rst_prolog = open(os.path.join(os.path.dirname(__file__), "substitutions.rst")).read()

# Add a substitution with links to download the docs in PDF or ZIP format.
# If building with ReadTheDocs, the link will be to the version that is being built.
# Otherwise it will be to the latest version.
doc_version_known = "READTHEDOCS_VERSION" in os.environ
offline_doc_version = os.environ.get("READTHEDOCS_VERSION", "latest")
pdf_url = f"/_/downloads/en/{offline_doc_version}/pdf/"
zip_url = f"/_/downloads/en/{offline_doc_version}/htmlzip/"
if doc_version_known:
    offline_download_text = "This documentation is also "
    url_root = ""
else:
    offline_download_text = "Documentation for the latest version of GDAL is "
    url_root = "https://gdal.org"
offline_download_text += f"available as a `PDF <{url_root}{pdf_url}>`__ or a `ZIP of individual HTML pages <{url_root}{zip_url}>`__ for offline browsing."
rst_prolog += f"""
.. |offline-download| replace:: {offline_download_text}
"""

source_suffix = {
    ".rst": "restructuredtext",
    ".ipynb": "myst-nb",
    ".myst": "myst-nb",
}

# -- Options for nitpicking -------------------------------------------------

nitpicky = True

nitpick_ignore = [
    # Standard C types or constants: no need to document them
    ("cpp:identifier", "__FILE__"),
    ("cpp:identifier", "__LINE__"),
    ("cpp:identifier", "exception"),
    ("cpp:identifier", "FALSE"),
    ("cpp:identifier", "FILE"),
    ("cpp:identifier", "int64_t"),
    ("cpp:identifier", "INT_MIN"),
    ("cpp:identifier", "size_t"),
    ("cpp:identifier", "time_t"),
    ("cpp:identifier", "tm"),
    ("cpp:identifier", "TRUE"),
    ("cpp:identifier", "uint8_t"),
    ("cpp:identifier", "uint32_t"),
    ("cpp:identifier", "uint64_t"),
    ("cpp:identifier", "va_list"),
    # ODBC specific
    ("cpp:identifier", "DWORD"),
    ("cpp:identifier", "HDBC"),
    ("cpp:identifier", "HENV"),
    ("cpp:identifier", "HSTMT"),
    ("cpp:identifier", "ODBC_FILENAME_MAX"),
    ("cpp:identifier", "ODBC_INSTALL_COMPLETE"),
    ("cpp:identifier", "SQL_FETCH_NEXT"),
    ("cpp:identifier", "SQL_MAX_MESSAGE_LENGTH"),
    ("cpp:identifier", "SQLSMALLINT"),
    ("cpp:identifier", "WORD"),
    # GEOS types
    ("cpp:identifier", "GEOSContextHandle_t"),
    ("cpp:identifier", "GEOSGeom"),
    # Arrow types
    ("cpp:identifier", "ArrowArray"),
    ("cpp:identifier", "ArrowArrayStream"),
    ("cpp:identifier", "ArrowSchema"),
    # Internal GDAL types
    ("cpp:identifier", "ConstIterator"),
    ("cpp:identifier", "GeomFields<OGRFeatureDefn*, OGRGeomFieldDefn*>"),
    ("cpp:identifier", "GeomFields<const OGRFeatureDefn*, const OGRGeomFieldDefn*>"),
    ("cpp:identifier", "FeatureIterator"),
    ("cpp:identifier", "Fields<OGRFeatureDefn*, OGRFieldDefn*>"),
    ("cpp:identifier", "Fields<const OGRFeatureDefn*, const OGRFieldDefn*>"),
    ("cpp:identifier", "GDALPamDataset"),
    ("cpp:identifier", "GDALPamRasterBand"),
    ("cpp:identifier", "GDALPluginDriverProxy"),
    ("cpp:identifier", "GUInt64VarArg"),
    ("cpp:identifier", "Iterator"),
    ("cpp:identifier", "OGRPointIterator"),
    ("cpp:identifier", "Private"),
    ("cpp:identifier", "TemporaryUnsealer"),
    ("cpp:identifier", "WindowIteratorWrapper"),
    ("cpp:class", "GDALPamDataset"),
    ("cpp:class", "GDALProxyDataset"),
    ("cpp:class", "GDALProxyRasterBand"),
    ("cpp:class", "RawDataset"),
    ("cpp:class", "RawRasterBand"),
    ("cpp:class", "VSICachedFile"),
    # Internal GDAL functions
    ("cpp:func", "GDALCheckBandCount"),
    ("cpp:func", "GDALCheckDatasetDimensions"),
    # Other
    ("envvar", "CFLAGS"),
    ("envvar", "CXXFLAGS"),
    # Python related
    ("py:class", "optional"),
    # TODO: To examine (ignoring might be the best option sometimes)
    ("c:enumerator", "OAMS_TRADITIONAL_GIS_ORDER"),
    ("c:enumerator", "OAMS_AUTHORITY_COMPLIANT"),
    ("c:enumerator", "OAMS_CUSTOM"),
    ("cpp:identifier", "gdal"),  # gdal C++ namespace
    ("cpp:identifier", "GDALAsyncReader"),
    ("cpp:identifier", "GDALComputedRasterBand"),
    ("cpp:identifier", "GDALSubdatasetInfo"),
    ("cpp:identifier", "GDALSuggestedBlockAccessPattern"),
    ("cpp:identifier", "GNMGFID"),
    ("cpp:identifier", "GNM_EDGE_DIR_BOTH"),
    ("cpp:identifier", "OGRFeatureUniquePtr"),
    ("cpp:identifier", "OGRSpatialReferenceReleaser"),
    ("cpp:identifier", "OGRStyleParamId"),
    ("cpp:identifier", "OGRStyleValue"),
    ("cpp:identifier", "string"),
    ("cpp:class", "GNMGdalNetwork"),
    ("cpp:class", "OGRStyleBrush"),
    ("cpp:class", "OGRStyleLabel"),
    ("cpp:class", "OGRStylePen"),
    ("cpp:class", "OGRStyleSymbol"),
    ("cpp:class", "VRTDataset"),
    ("cpp:class", "VSIFilesystemHandler"),
    ("cpp:func", "CPLFetchNameValue"),
    ("cpp:func", "GDALDataset::BlockBasedRasterIO"),
    ("cpp:func", "GDALDataset::GetMetadata"),
    ("cpp:func", "GDALDataset::GetMetadataItem"),
    ("cpp:func", "GDALDataset::ICreateLayer"),
    ("cpp:func", "GDALDataset::TryLoadXML"),
    ("cpp:func", "GDALDriver::SetDescription"),
    ("cpp:func", "GDALRasterBand::EnablePixelTypeSignedByteWarning"),
    ("cpp:func", "GDALRasterBand::GetMetadata"),
    ("cpp:func", "OGRGetXML_UTF8_EscapedString"),
    ("cpp:func", "OGR_G_GetBoundary"),
    ("cpp:func", "OGR_G_SymmetricDifference"),
    ("cpp:func", "OGR_L_GetFeaturesRead"),
    ("cpp:func", "OGR_L_GetRefCount"),
    ("cpp:func", "OGRLayer::ISetFeature"),
    ("cpp:func", "OGRLayerDefn::AddFieldDefn"),
    ("cpp:func", "OGRLayerDefn::DeleteFieldDefn"),
    ("cpp:func", "OGRLineString::transform"),
    ("cpp:func", "wkbFlatten"),
    ("cpp:member", "OGRLayer::m_poAttrQuery"),
    ("cpp:member", "OGRLayer::m_poFilterGeom"),
    # TODO (low priority): Below could potentially be fixed
    ("cpp:identifier", "CPLJSONObject"),
    ("cpp:identifier", "CPLHTTPFetchWriteFunc"),
    ("cpp:identifier", "CPLLockFileStruct"),
    ("cpp:identifier", "CPL_MUTEX_RECURSIVE"),
    ("cpp:identifier", "CPLXMLTreeCloserDeleter"),
    ("cpp:identifier", "GDALMaskFunc"),
    ("cpp:identifier", "GDALRawResult"),
    ("cpp:identifier", "GDALTransformerUniquePtrReleaser"),
    ("cpp:identifier", "GDALWarpChunk"),
    ("cpp:func", "OGRGeocode"),
    ("cpp:func", "OGRGeocodeCreateSession"),
    ("cpp:func", "OGRGeocodeReverse"),
    ("cpp:identifier", "OGRGeomTransformer"),
    ("cpp:identifier", "USGS_ANGLE_PACKEDDMS"),
    ("cpp:identifier", "VSIStatBuf"),
    ("cpp:identifier", "VSIStatBufL"),
]

nitpick_ignore_regex = [
    (".*", "cpl.*_8h.*"),
    (".*", "deprecated_.*"),
    (".*", "gdal.*_8h.*"),
    (".*", "gnm.*_8h.*"),
    (".*", "ogr.*_8h.*"),
    ("cpp:identifier", "_CPL.*"),  # opaque types
    ("cpp:identifier", "_OGR.*"),  # opaque types
    ("cpp:identifier", "GDAL.*HS"),  # opaque types
    ("cpp:identifier", "OGR.*HS"),  # opaque types
    # Deprecated classes
    (".*", "classOGRDataSource"),
    (".*", "classOGRSFDriver"),
    # Internal GDAL types
    (".*", "classAxisMappingCoordinateTransformation"),
    (".*", "classCompositeCT"),
    (".*", "classCutlineTransformer"),
    (".*", "classGCPCoordTransformation"),
    (".*", "classGeoTransformCoordinateTransformation"),
    (".*", "classGDALApplyVSGDataset"),
    (".*", "classGDALApplyVSGRasterBand"),
    (".*", "classGDALAsyncReader_.*"),
    (".*", "classGDALColorReliefDataset"),
    (".*", "classGDALColorReliefRasterBand"),
    (".*", "classGDALComputedDataset"),
    (".*", "classGDALDatasetAlgorithm"),
    (".*", "classGDALFootprintCombinedMaskBand"),
    (".*", "classGDALFootprintMaskBand"),
    (".*", "classGDALGeneric3x3Dataset"),
    (".*", "classGDALGeneric3x3RasterBand"),
    (".*", "classGDALInConstructionAlgorithmArg"),
    (".*", "classGDALMDArrayFromDataset"),
    (".*", "classGDALMDArrayFromRasterBand"),
    (".*", "classGDALMDArrayMeshGrid"),
    (".*", "classGDALMDArrayResampledDatasetRasterBand"),
    (".*", "classGDALMdimAlgorithm"),
    (".*", "classGDALRasterAlgorithm"),
    (".*", "classGDALVectorAlgorithm"),
    (".*", "classGDALVSIAlgorithm"),
    (".*", "classOGRPointIterator_.*"),
    (".*", "classGDALOverviewDataset"),
    (".*", "classGDALPamDataset"),
    (".*", "classGDALPamRasterBand"),
    (".*", "classGDALPluginDriverProxy"),
    (".*", "classGDALRasterAttributeTableFromMDArrays"),
    (".*", "classGDALVectorTranslateWrappedDataset"),
    (".*", "classOGRDefaultConstGeometryVisitor"),
    (".*", "classOGRDefaultGeometryVisitor"),
    (".*", "classOGRIteratedPoint"),
    (".*", "classOGRSplitListFieldLayer"),
    (".*", "structOGRSpatialReference_.*"),
    (".*", "classPythonPluginDataset"),
    (".*", "classPythonPluginLayer"),
    (".*", "classPythonPluginDriver"),
    (".*", "classGDALSubsetGroup"),
    (".*", "structOGRwkbExportOptions"),  # only emitted by Windows CI
    # FIXME We ignore everything python related for now...
    ("py:.*", ".*"),
    # TODO: To examine
    (".*", "classGDALDataset_.*"),
    (".*", "classGDALIHasAttribute_.*"),
    (".*", "classOGRLayer_.*"),
    # TODO: Below could potentially be fixed
    (".*", "classGDALAsyncReader"),
    (".*", "classGDALComputedRasterBand"),
    (".*", "classGDALMDArrayFromRasterBand_1_1MDIAsAttribute"),
    (".*", "classVSISparseFileHandle"),
    (".*", "classVSISubFileHandle"),
    (".*", "classVSIUploadOnCloseHandle"),
    (".*", "structGDALSubdatasetInfo"),
]

# -- Options for HTML output -------------------------------------------------

# The theme to use for HTML and HTML Help pages.  See the documentation for
# a list of builtin themes.
#
html_theme_path = ["."]
html_theme = "gdal_rtd"

html_context = {
    "display_github": True,
    "theme_vcs_pageview_mode": "edit",
    "github_user": "OSGeo",
    "github_repo": "gdal",
    "github_version": "master",
    "conf_py_path": "/doc/source/",
}

html_theme_options = {
    "canonical_url": "https://gdal.org/",  # Trailing slash needed to have correct <link rel="canonical" href="..."/> URLs
    "analytics_id": "",  #  Provided by Google in your dashboard
    "logo_only": True,
    "version_selector": True,
    "prev_next_buttons_location": "both",
    "style_external_links": False,
    #'vcs_pageview_mode': '',
    "style_nav_header_background": "white",
    # Toc options
    "collapse_navigation": True,
    "sticky_navigation": True,
    #'navigation_depth': 4,
    "includehidden": True,
    "titles_only": False,
    "flyout_display": "attached",
}

# Add any paths that contain custom static files (such as style sheets) here,
# relative to this directory. They are copied after the builtin static files,
# so a file named "default.css" will overwrite the builtin "default.css".
html_static_path = ["_static"]

# For generated content and robots.txt
html_extra_path = [os.path.join(build_dir, "html_extra"), "extra_path"]

# If true, links to the reST sources are added to the pages.
html_show_sourcelink = False

html_logo = "../images/gdalicon.png"

html_favicon = "../images/favicon.png"

# -- Options for manual page output ---------------------------------------

# One entry per manual page. List of tuples
# (source start file, name, description, authors, manual section).

author_frankw = "Frank Warmerdam <warmerdam@pobox.com>"
author_silker = "Silke Reimer <silke@intevation.de>"
author_mikhailg = "Mikhail Gusev <gusevmihs@gmail.com>"
author_dbaston = "Dan Baston <dbaston@gmail.com>"
author_dmitryb = "Dmitry Baryshnikov <polimax@mail.ru>"
author_evenr = "Even Rouault <even.rouault@spatialys.com>"
author_elpaso = "Alessandro Pasotti <elpaso@itopen.it>"
author_tamass = "Tamas Szekeres <szekerest@gmail.com>"

man_pages = [
    # New gdal commands and subcommands
    (
        "programs/gdal",
        "gdal",
        "Main gdal entry point",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_info",
        "gdal-info",
        "Get information on a dataset",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_convert",
        "gdal-convert",
        "Convert a dataset",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_dataset",
        "gdal-dataset",
        "Entry point for dataset management commands",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_dataset_identify",
        "gdal-dataset-identify",
        "Identify driver opening dataset(s)",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_dataset_check",
        "gdal-dataset-check",
        "Check whether there are errors when reading the content of a dataset",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_dataset_copy",
        "gdal-dataset-copy",
        "Copy files of a dataset",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_dataset_rename",
        "gdal-dataset-rename",
        "Rename files of a dataset",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_dataset_delete",
        "gdal-dataset-delete",
        "Delete dataset(s)",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_mdim",
        "gdal-mdim",
        "Entry point for multidimensional commands",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_mdim_info",
        "gdal-mdim-info",
        "Get information on a multidimensional dataset",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_mdim_convert",
        "gdal-mdim-convert",
        "Convert a multidimensional dataset",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_mdim_mosaic",
        "gdal-mdim-mosaic",
        "Build a mosaic, either virtual (VRT) or materialized, from multidimensional datasets",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_pipeline",
        "gdal-pipeline",
        "Process a dataset applying several steps",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_raster",
        "gdal-raster",
        "Entry point for raster commands",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_raster_info",
        "gdal-raster-info",
        "Get information on a raster dataset",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_raster_set_type",
        "gdal-raster-set-type",
        "Modify the data type of bands of a raster dataset",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_raster_as_features",
        "gdal-raster-as-features",
        "Create features representing the pixels of a raster",
        [author_dbaston],
        1,
    ),
    (
        "programs/gdal_raster_aspect",
        "gdal-raster-aspect",
        "Generate an aspect map",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_raster_blend",
        "gdal-raster-color-blend",
        "Use a grayscale raster to replace the intensity of a RGB/RGBA dataset",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_raster_calc",
        "gdal-raster-calc",
        "Perform pixel-wise calculations on a raster",
        [author_dbaston],
        1,
    ),
    (
        "programs/gdal_raster_clean_collar",
        "gdal-raster-clean-collar",
        "Clean the collar of a raster dataset, removing noise",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_raster_clip",
        "gdal-raster-clip",
        "Clip a raster dataset",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_raster_color_map",
        "gdal-raster-color-map",
        "Generate a RGB or RGBA dataset from a single band, using a color map",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_raster_contour",
        "gdal-raster-contour",
        "Creates a vector contour from a raster elevation model (DEM)",
        [author_elpaso],
        1,
    ),
    (
        "programs/gdal_raster_compare",
        "gdal-raster-compare",
        "Compare two raster dataset",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_raster_convert",
        "gdal-raster-convert",
        "Convert a raster dataset",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_raster_create",
        "gdal-raster-create",
        "Create a new raster dataset",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_raster_edit",
        "gdal-raster-edit",
        "Edit in place a raster dataset",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_raster_footprint",
        "gdal-raster-footprint",
        "Compute the footprint of a raster dataset",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_raster_fill_nodata",
        "gdal-raster-fill-nodata",
        "Fill nodata values in a raster dataset",
        [author_elpaso],
        1,
    ),
    (
        "programs/gdal_raster_hillshade",
        "gdal-raster-hillshade",
        "Generate a shaded relief map",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_raster_index",
        "gdal-raster-index",
        "Create a vector index of raster datasets",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_raster_mosaic",
        "gdal-raster-mosaic",
        "Build a mosaic",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_raster_neighbors",
        "gdal-raster-neighbors",
        "Compute the value of each pixel from its neighbors (focal statistics)",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_raster_nodata_to_alpha",
        "gdal-raster-nodata-to-alpha",
        "Replace nodata value(s) with an alpha band",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_raster_overview_add",
        "gdal-raster-overview-add",
        "Add overviews to a raster dataset",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_raster_overview_delete",
        "gdal-raster-overview-delete",
        "Delete overviews of a raster dataset",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_raster_overview_refresh",
        "gdal-raster-overview-refresh",
        "Refresh overviews",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_raster_pansharpen",
        "gdal-raster-pansharpen",
        "Perform a pansharpen operation",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_raster_pipeline",
        "gdal-raster-pipeline",
        "Process a raster dataset applying several steps",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_raster_polygonize",
        "gdal-raster-polygonize",
        "Create a polygon feature dataset from a raster band",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_raster_pixel_info",
        "gdal-raster-pixel-info",
        "Return information on a pixel of a raster dataset",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_raster_proximity",
        "gdal-raster-proximity",
        "Produces a raster proximity map",
        [author_elpaso],
        1,
    ),
    (
        "programs/gdal_raster_reclassify",
        "gdal-raster-reclassify",
        "Reclassify a raster dataset",
        [author_dbaston],
        1,
    ),
    (
        "programs/gdal_raster_reproject",
        "gdal-raster-reproject",
        "Reproject a raster dataset",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_raster_resize",
        "gdal-raster-resize",
        "Resize a raster dataset",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_raster_rgb_to_palette",
        "gdal-raster-rgb-to-palette",
        "Convert a RGB image into a pseudo-color / paletted image.",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_raster_roughness",
        "gdal-raster-roughness",
        "Generate a roughness map",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_raster_scale",
        "gdal-raster-scale",
        "Scale the values of the bands of a raster dataset",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_raster_sieve",
        "gdal-raster-sieve",
        "Remove small raster polygons",
        [author_elpaso],
        1,
    ),
    (
        "programs/gdal_raster_select",
        "gdal-raster-select",
        "Select a subset of bands from a raster dataset",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_raster_slope",
        "gdal-raster-slope",
        "Generate a slope map",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_raster_stack",
        "gdal-raster-stack",
        "Combine together input bands into a multi-band output, either virtual (VRT) or materialized",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_raster_tile",
        "gdal-raster-tile",
        "Generate tiles in separate files from a raster dataset",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_raster_tpi",
        "gdal-raster-tpi",
        "Generate a Topographic Position Index (TPI) map",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_raster_tri",
        "gdal-raster-tri",
        "Generate a Terrain Ruggedness Index (TRI) map",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_raster_unscale",
        "gdal-raster-unscale",
        "Convert scaled values of a raster dataset into unscaled values",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_raster_update",
        "gdal-raster-update",
        "Update the destination raster with the content of the input one.",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_raster_viewshed",
        "gdal-raster-viewshed",
        "Compute the viewshed of a raster dataset.",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_raster_zonal_stats",
        "gdal-raster-zonal-stats",
        "Compute raster zonal statistics.",
        [author_dbaston],
        1,
    ),
    (
        "programs/gdal_vector",
        "gdal-vector",
        "Entry point for vector commands",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_vector_info",
        "gdal-vector-info",
        "Get information on a vector dataset",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_vector_check_coverage",
        "gdal-vector-check-coverage",
        "Check polygon coverage for validity",
        [author_dbaston],
        1,
    ),
    (
        "programs/gdal_vector_check_geometry",
        "gdal-vector-check-geometry",
        "Check a dataset for invalid or non-simple geometries",
        [author_dbaston],
        1,
    ),
    (
        "programs/gdal_vector_clean_coverage",
        "gdal-vector-clean-coverage",
        "Remove gaps and overlaps from a polygon dataset",
        [author_dbaston],
        1,
    ),
    (
        "programs/gdal_vector_clip",
        "gdal-vector-clip",
        "Clip a vector dataset",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_vector_concat",
        "gdal-vector-concat",
        "Concatenate vector datasets",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_vector_convert",
        "gdal-vector-convert",
        "Convert a vector dataset",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_vector_edit",
        "gdal-vector-edit",
        "Edit metadata of a vector dataset",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_vector_filter",
        "gdal-vector-filter",
        "Filter a vector dataset",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_vector_explode_collections",
        "gdal-vector-explode-collections",
        "Explode geometries of type collection of a vector dataset",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_vector_make_point",
        "gdal-vector-make-point",
        "Create point features from attribute fields",
        [author_dbaston],
        1,
    ),
    (
        "programs/gdal_vector_make_valid",
        "gdal-vector-make-valid",
        "Fix validity of geometries of a vector dataset",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_vector_segmentize",
        "gdal-vector-segmentize",
        "Segmentize geometries of a vector dataset",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_vector_simplify",
        "gdal-vector-simplify",
        "Simplify geometries of a vector dataset",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_vector_sort",
        "gdal-vector-sort",
        "Spatially sort a vector dataset",
        [author_dbaston],
        1,
    ),
    (
        "programs/gdal_vector_buffer",
        "gdal-vector-buffer",
        "Compute a buffer around geometries of a vector dataset",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_vector_swap_xy",
        "gdal-vector-swap-xy",
        "Swap X and Y coordinates of geometries of a vector dataset",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_vector_grid",
        "gdal-vector-grid",
        "Create a regular grid from scattered points",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_vector_index",
        "gdal-vector-index",
        "Create a vector index of vector datasets",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_vector_layer_algebra",
        "gdal-vector-layer-algebra",
        "Perform algebraic operation between 2 layers",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_vector_partition",
        "gdal-vector-partition",
        "Partition a vector dataset into multiple files",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_vector_pipeline",
        "gdal-vector-pipeline",
        "Process a vector dataset applying several steps",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_vector_rasterize",
        "gdal-vector-rasterize",
        "Burn vector geometries into a raster",
        [author_elpaso],
        1,
    ),
    (
        "programs/gdal_vector_select",
        "gdal-vector-select",
        "Select a subset of fields from a vector dataset",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_vector_set_field_type",
        "gdal-vector-set-field-type",
        "Modify the type of a field of a vector dataset",
        [author_elpaso],
        1,
    ),
    (
        "programs/gdal_vector_set_geom_type",
        "gdal-vector-set-geom-type",
        "Modify the geometry type of a vector dataset",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_vector_sql",
        "gdal-vector-sql",
        "Apply SQL statement(s) to a dataset",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_vector_update",
        "gdal-vector-update",
        "Update an existing vector dataset with an input vector dataset",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_vsi",
        "gdal-vsi",
        "Entry point for GDAL Virtual System Interface (VSI) commands",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_vsi_copy",
        "gdal-vsi-copy",
        "Copy files located on GDAL Virtual System Interface (VSI)",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_vsi_delete",
        "gdal-vsi-delete",
        "Delete files located on GDAL Virtual System Interface (VSI)",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_vsi_list",
        "gdal-vsi-list",
        "List files of one of the GDAL Virtual System Interface (VSI)",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_vsi_move",
        "gdal-vsi-move",
        "Move/rename a file/directory located on GDAL Virtual System Interface (VSI)",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_vsi_sync",
        "gdal-vsi-sync",
        "Synchronize source and target file/directory located on GDAL Virtual System Interface (VSI)",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_vsi_sozip",
        "gdal-vsi-sozip",
        "SOZIP (Seek-Optimized ZIP) related commands.",
        [author_evenr],
        1,
    ),
    # Traditional utilities
    (
        "programs/gdalinfo",
        "gdalinfo",
        "Lists various information about a GDAL supported raster dataset",
        [author_frankw],
        1,
    ),
    (
        "programs/gdalmdiminfo",
        "gdalmdiminfo",
        "Reports structure and content of a multidimensional dataset",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_translate",
        "gdal_translate",
        "Converts raster data between different formats.",
        [author_frankw, author_silker],
        1,
    ),
    (
        "programs/gdalmdimtranslate",
        "gdalmdimtranslate",
        "Converts multidimensional data between different formats, and perform subsetting.",
        [author_evenr],
        1,
    ),
    (
        "programs/gdaladdo",
        "gdaladdo",
        "Builds or rebuilds overview images.",
        [author_frankw, author_silker],
        1,
    ),
    (
        "programs/gdalwarp",
        "gdalwarp",
        "Image reprojection and warping utility.",
        [author_frankw, author_silker],
        1,
    ),
    (
        "programs/gdaltindex",
        "gdaltindex",
        "Builds a shapefile as a raster tileindex.",
        [author_frankw],
        1,
    ),
    (
        "programs/gdalbuildvrt",
        "gdalbuildvrt",
        "Builds a VRT from a list of datasets.",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_contour",
        "gdal_contour",
        "Builds vector contour lines from a raster elevation model.",
        [author_frankw, author_silker],
        1,
    ),
    (
        "programs/gdaldem",
        "gdaldem",
        "Tools to analyze and visualize DEMs.",
        [
            "Matthew Perry <perrygeo@gmail.com>",
            author_evenr,
            "Howard Butler <hobu.inc@gmail.com>",
            "Chris Yesson <chris.yesson@ioz.ac.uk>",
        ],
        1,
    ),
    (
        "programs/gdal_viewshed",
        "gdal_viewshed",
        "Calculates a viewshed raster from an input raster DEM for a user defined point",
        [author_tamass],
        1,
    ),
    (
        "programs/gdal_create",
        "gdal_create",
        "Create a raster file (without source dataset)",
        [author_evenr],
        1,
    ),
    (
        "programs/rgb2pct",
        "rgb2pct",
        "Convert a 24bit RGB image to 8bit paletted.",
        [author_frankw],
        1,
    ),
    (
        "programs/pct2rgb",
        "pct2rgb",
        "Convert an 8bit paletted image to 24bit RGB.",
        [author_frankw, author_silker],
        1,
    ),
    (
        "programs/gdal_merge",
        "gdal_merge",
        "Mosaics a set of images.",
        [author_frankw, author_silker],
        1,
    ),
    (
        "programs/gdal2tiles",
        "gdal2tiles",
        "Generates directory with TMS tiles, KMLs and simple web viewers.",
        ["Klokan Petr Pridal <klokan@klokan.cz>"],
        1,
    ),
    (
        "programs/gdal_rasterize",
        "gdal_rasterize",
        "Burns vector geometries into a raster.",
        [author_frankw],
        1,
    ),
    (
        "programs/gdaltransform",
        "gdaltransform",
        "Transforms coordinates.",
        [author_frankw, "Jan Hartmann <j.l.h.hartmann@uva.nl>"],
        1,
    ),
    (
        "programs/nearblack",
        "nearblack",
        "Convert nearly black/white borders to black.",
        [author_frankw],
        1,
    ),
    (
        "programs/gdal_retile",
        "gdal_retile",
        "Retiles a set of tiles and/or build tiled pyramid levels.",
        ["Christian Mueller <christian.mueller@nvoe.at>"],
        1,
    ),
    (
        "programs/gdal_grid",
        "gdal_grid",
        "Creates regular grid from the scattered data.",
        ["Andrey Kiselev <dron@ak4719.spb.edu>"],
        1,
    ),
    (
        "programs/gdal_proximity",
        "gdal_proximity",
        "Produces a raster proximity map.",
        [author_frankw],
        1,
    ),
    (
        "programs/gdal_polygonize",
        "gdal_polygonize",
        "Produces a polygon feature layer from a raster.",
        [author_frankw],
        1,
    ),
    (
        "programs/gdal_sieve",
        "gdal_sieve",
        "Removes small raster polygons.",
        [author_frankw],
        1,
    ),
    (
        "programs/gdal_fillnodata",
        "gdal_fillnodata",
        "Fill raster regions by interpolation from edges.",
        [author_frankw],
        1,
    ),
    (
        "programs/gdallocationinfo",
        "gdallocationinfo",
        "Raster query tool.",
        [author_frankw],
        1,
    ),
    (
        "programs/gdalsrsinfo",
        "gdalsrsinfo",
        "Lists info about a given SRS in number of formats (WKT, PROJ.4, etc.)",
        [author_frankw, "Etienne Tourigny <etourigny.dev-at-gmail-dot-com>"],
        1,
    ),
    (
        "programs/gdalmove",
        "gdalmove",
        "Transform georeferencing of raster file in place.",
        [author_frankw],
        1,
    ),
    (
        "programs/gdal_edit",
        "gdal_edit",
        "Edit in place various information of an existing GDAL dataset.",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_calc",
        "gdal_calc",
        "Command line raster calculator with numpy syntax.",
        [
            "Chris Yesson <chris dot yesson at ioz dot ac dot uk>",
            "Etienne Tourigny <etourigny dot dev at gmail dot com>",
        ],
        1,
    ),
    (
        "programs/gdal_pansharpen",
        "gdal_pansharpen",
        " Perform a pansharpen operation.",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal-config",
        "gdal-config",
        " Determines various information about a GDAL installation.",
        [author_frankw],
        1,
    ),
    (
        "programs/gdalmanage",
        "gdalmanage",
        " Identify, delete, rename and copy raster data files.",
        [author_frankw],
        1,
    ),
    ("programs/gdalcompare", "gdalcompare", " Compare two images.", [author_frankw], 1),
    (
        "programs/ogrinfo",
        "ogrinfo",
        "Lists information about an OGR-supported data source.",
        [author_frankw, author_silker],
        1,
    ),
    (
        "programs/ogr2ogr",
        "ogr2ogr",
        "Converts simple features data between file formats.",
        [author_frankw, author_silker],
        1,
    ),
    (
        "programs/ogrtindex",
        "ogrtindex",
        "Creates a tileindex.",
        [author_frankw, author_silker],
        1,
    ),
    (
        "programs/ogrlineref",
        "ogrlineref",
        "Create linear reference and provide some calculations using it.",
        [author_dmitryb],
        1,
    ),
    (
        "programs/ogrmerge",
        "ogrmerge",
        " Merge several vector datasets into a single one.",
        [author_evenr],
        1,
    ),
    (
        "programs/gnmmanage",
        "gnmmanage",
        "Manages networks",
        [author_mikhailg, author_dmitryb],
        1,
    ),
    (
        "programs/gnmanalyse",
        "gnmanalyse",
        "Analyses networks",
        [author_mikhailg, author_dmitryb],
        1,
    ),
    (
        "programs/ogr_layer_algebra",
        "ogr_layer_algebra",
        "Performs various Vector layer algebraic operations",
        [],
        1,
    ),
    (
        "programs/sozip",
        "sozip",
        "Generate a seek-optimized (SOZip) file.",
        [author_evenr],
        1,
    ),
    (
        "programs/gdal_footprint",
        "gdal_footprint",
        "Compute footprint of a raster.",
        [author_evenr],
        1,
    ),
]


# latex

preamble = r"""
\ifdefined\DeclareUnicodeCharacter
  \DeclareUnicodeCharacter{2032}{$'$}% prime
  \DeclareUnicodeCharacter{200B}{{\hskip 0pt}}
\fi
"""

# Package substitutefont no longer exists since TeXLive 2023 later than August 2023
# and has been replaced with sphinxpackagesubstitutefont
# https://github.com/jfbu/sphinx/commit/04cbd819b0e285d058549b2173af7efadf1cd020
import sphinx

if os.path.exists(
    os.path.join(
        os.path.dirname(sphinx.__file__), "texinputs", "sphinxpackagesubstitutefont.sty"
    )
):
    substitutefont_package = "sphinxpackagesubstitutefont"
else:
    substitutefont_package = "substitutefont"

latex_elements = {
    # The paper size ('letterpaper' or 'a4paper').
    #'papersize': 'letterpaper',
    # The font size ('10pt', '11pt' or '12pt').
    #'pointsize': '10pt',
    # Additional stuff for the LaTeX preamble.
    "preamble": preamble,
    "inputenc": "\\usepackage[utf8]{inputenc}\n\\usepackage{CJKutf8}\n\\usepackage{"
    + substitutefont_package
    + "}",
    "babel": "\\usepackage[russian,main=english]{babel}\n\\selectlanguage{english}",
    "fontenc": "\\usepackage[LGR,X2,T1]{fontenc}",
    # Latex figure (float) alignment
    #'figure_align': 'htbp',
}

latex_documents = [
    ("index_pdf", "gdal.tex", project + " Documentation", author, "manual"),
]

latex_toplevel_sectioning = "chapter"

latex_logo = "../images/gdalicon_big.png"
# Disable module and domain indices in PDF output.
# Python API documentation is not included in the PDF, so keeping them
# results in a dummy and confusing "Python Module Index" section.
latex_use_modindex = False
latex_domain_indices = False


# If true, show URL addresses after external links.
# man_show_urls = False

# -- Breathe -------------------------------------------------

# Setup the breathe extension

breathe_projects = {"api": os.path.join(build_dir, "xml")}
breathe_default_project = "api"

# Tell sphinx what the primary language being documented is.
primary_domain = "cpp"

# -- Source file links ------------------------------------------

source_file_root = os.path.join(os.path.dirname(__file__), os.pardir, os.pardir)
source_file_url_template = "https://github.com/OSGeo/gdal/blob/master/{}"

# -- ReadTheDocs configuration ----------------------------------

# see https://about.readthedocs.com/blog/2024/07/addons-by-default/

# Define the canonical URL if you are using a custom domain on Read the Docs
html_baseurl = os.environ.get("READTHEDOCS_CANONICAL_URL", "")

# Tell Jinja2 templates the build is running on Read the Docs
if os.environ.get("READTHEDOCS", "") == "True":
    html_context["READTHEDOCS"] = True

# -- GDAL Config option listing ------------------------------------------
options_since_ignore_before = "3.0"

# -- Spelling --------------------------------------------------

# Avoid running git
spelling_ignore_contributor_names = False

spelling_word_list_filename = ["spelling_wordlist.txt"]

# -- myst-nb --------------------------------------------------

# Sets `text/plain` as the highest priority for `spelling` output.
nb_mime_priority_overrides = [
    ("spelling", "text/plain", 0),
]

# -- copy data files -----------------------------------------------------

data_dir = os.path.join(os.path.dirname(__file__), "..", "data")
os.makedirs(data_dir, exist_ok=True)
data_dir_with_stats = os.path.join(data_dir, "with_stats")
os.makedirs(data_dir_with_stats, exist_ok=True)

target_filename = os.path.join(data_dir, "utmsmall.tif")
shutil.copy(
    os.path.join(os.path.dirname(__file__), "../../autotest/gcore/data/utmsmall.tif"),
    target_filename,
)
if os.path.exists(target_filename + ".aux.xml"):
    os.unlink(target_filename + ".aux.xml")

target_filename = os.path.join(data_dir_with_stats, "utmsmall.tif")
shutil.copy(
    os.path.join(os.path.dirname(__file__), "../../autotest/gcore/data/utmsmall.tif"),
    target_filename,
)


def builder_inited(app):

    if app.builder.name == "html":
        check_python_bindings()


def setup(app):
    app.connect("builder-inited", builder_inited)
