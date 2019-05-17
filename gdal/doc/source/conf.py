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
import os
import sys
sys.path.insert(0, os.path.abspath('_extensions'))


# -- Project information -----------------------------------------------------

project = 'GDAL'
copyright = '1998-2019'
author = 'Frank Warmerdam, Even Rouault, and others'


# -- General configuration ---------------------------------------------------

# Add any Sphinx extension module names here, as strings. They can be
# extensions coming with Sphinx (named 'sphinx.ext.*') or your custom
# ones.
extensions = [
    'breathe',
    'configoptions'
]

# Add any paths that contain templates here, relative to this directory.
templates_path = ['_templates']

# List of patterns, relative to source directory, that match files and
# directories to ignore when looking for source files.
# This pattern also affects html_static_path and html_extra_path.
exclude_patterns = []

# -- Options for HTML output -------------------------------------------------

# The theme to use for HTML and HTML Help pages.  See the documentation for
# a list of builtin themes.
#
html_theme_path = ['.']
html_theme = 'gdal_rtd'
html_theme_options = {
        'canonical_url': 'https://gdal.dev',
    'analytics_id': '',  #  Provided by Google in your dashboard
    'logo_only': True,
    'display_version': True,
    'prev_next_buttons_location': 'both',
    'style_external_links': False,
    #'vcs_pageview_mode': '',
    'style_nav_header_background': 'white',
    # Toc options
    'collapse_navigation': True,
    'sticky_navigation': True,
    #'navigation_depth': 4,
    'includehidden': True,
    'titles_only': False
}

# Add any paths that contain custom static files (such as style sheets) here,
# relative to this directory. They are copied after the builtin static files,
# so a file named "default.css" will overwrite the builtin "default.css".
html_static_path = ['_static']

# If true, links to the reST sources are added to the pages.
html_show_sourcelink = False

html_logo = '../images/gdalicon.png'

# -- Options for manual page output ---------------------------------------

# One entry per manual page. List of tuples
# (source start file, name, description, authors, manual section).
man_pages = [
    (
        'programs/gdalinfo',
        'gdalinfo',
        u'Lists various information about a GDAL supported raster dataset',
        ['Frank Warmerdam'],
        1
    ),
    (
        'programs/gnmmanage',
        'gnmmanage',
        u'Manages networks',
        ['Mikhail Gusev <gusevmihs@gmail.com>','Dmitry Baryshnikov <polimax@mail.ru>'],
        1
    ),
]

# If true, show URL addresses after external links.
#man_show_urls = False

# -- Breathe -------------------------------------------------

# Setup the breathe extension
breathe_projects = {
    "api": "../build/xml"
}
breathe_default_project = "api"

# Tell sphinx what the primary language being documented is.
primary_domain = 'cpp'
