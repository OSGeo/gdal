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
import sys

sys.path.insert(0, os.path.abspath("_extensions"))

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
    gdal_version = gdal.__version__
    gdal_version_stripped = gdal_version
    pos_dev = gdal_version_stripped.find("dev")
    if pos_dev > 0:
        gdal_version_stripped = gdal_version_stripped[0:pos_dev]

    if doc_version.strip() != gdal_version_stripped:
        logger.warn(
            f"Building documentation for GDAL {doc_version} but osgeo.gdal module has version {gdal_version}. Python API documentation may be incorrect."
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
    "redirects",
    "driverproperties",
    "source_file",
    "sphinx.ext.napoleon",
    "sphinxcontrib.jquery",
]

# Add any paths that contain templates here, relative to this directory.
templates_path = ["_templates"]

# List of patterns, relative to source directory, that match files and
# directories to ignore when looking for source files.
# This pattern also affects html_static_path and html_extra_path.
exclude_patterns = ["programs/options/*.rst", "api/python/modules.rst"]

# Prevents double hyphen (--) to be replaced by Unicode long dash character
# Cf https://stackoverflow.com/questions/15258831/how-to-handle-two-dashes-in-rest
smartquotes = False

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
    "github_version": "master/doc/source/",
}

html_theme_options = {
    "canonical_url": "https://gdal.org/",  # Trailing slash needed to have correct <link rel="canonical" href="..."/> URLs
    "analytics_id": "",  #  Provided by Google in your dashboard
    "logo_only": True,
    "display_version": True,
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
}

# Add any paths that contain custom static files (such as style sheets) here,
# relative to this directory. They are copied after the builtin static files,
# so a file named "default.css" will overwrite the builtin "default.css".
# html_static_path = ['_static']

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
author_dmitryb = "Dmitry Baryshnikov <polimax@mail.ru>"
author_evenr = "Even Rouault <even.rouault@spatialys.com>"
author_tamass = "Tamas Szekeres <szekerest@gmail.com>"

man_pages = [
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
    "fontenc": "\\usepackage[LGR,X2,T1]{fontenc}"
    # Latex figure (float) alignment
    #'figure_align': 'htbp',
}

latex_documents = [
    ("index_pdf", "gdal.tex", project + " Documentation", author, "manual"),
]

latex_toplevel_sectioning = "chapter"

latex_logo = "../images/gdalicon_big.png"

# If true, show URL addresses after external links.
# man_show_urls = False

# -- Breathe -------------------------------------------------

# Setup the breathe extension
breathe_projects = {"api": "../build/xml"}
breathe_default_project = "api"

# Tell sphinx what the primary language being documented is.
primary_domain = "cpp"

# -- Source file links ------------------------------------------

source_file_root = os.path.join(os.path.dirname(__file__), os.pardir, os.pardir)
source_file_url_template = "https://github.com/OSGeo/gdal/blob/master/{}"

# -- GDAL Config option listing ------------------------------------------
options_since_ignore_before = "3.0"

# -- Redirects --------------------------------------------------

enable_redirects = False
