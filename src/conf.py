"""
Configuration file for the Sphinx documentation builder.

For the full list of built-in configuration values, see the documentation:
https://www.sphinx-doc.org/en/master/usage/configuration.html
"""

import os
import sys
from datetime import date

# %% Project information

project = "GDAL CLI Tutorial Workshop"
title = "GDAL CLI Tutorial Workshop"
author = "Even Rouault"

today_date = date.today() # or use a specific date
#today_date = date(2026, 3, 1)
today_fmt = "%d %b %Y"

copyright = "2026, Even Rouault".format(today_date.year)

# %% General configuration

# Sphinx extension modules
extensions = [
    "sphinx.ext.githubpages",  # to generate .nojekyll
    "sphinx_toolbox.collapse",
]

# File extensions of source files
source_suffix = {
    ".rst": "restructuredtext",
}

# Document name that contains the root toctree directive
root_doc = "index"

# A list of patterns that should be excluded when looking for source files
exclude_patterns = ["operations/options/*.rst"]

# A list of paths that contain extra templates
templates_path = ["_templates"]

# Prevents double hyphen (--) to be replaced by Unicode long dash character
# Cf https://stackoverflow.com/questions/15258831/how-to-handle-two-dashes-in-rest
smartquotes = False

# The current date, used as the replacement for |today|
today = today_date.strftime(today_fmt)

# The default language to highlight source code
highlight_language = "none"

# The name of the Pygments (syntax highlighting) style to use
pygments_style = "sphinx"

# %% Extension configurations

# Spelling configuration
spelling_lang = tokenizer_lang = "en_GB"
spelling_show_suggestions = False
spelling_ignore_importable_modules = False

# %% Options for HTML output

# The theme to use for HTML and HTML Help pages
html_theme = "sphinx_rtd_theme"

# Theme options are theme-specific and customize the look and feel of a theme
html_theme_options = {
    "canonical_url": "https://gdal-cli-workshop.github.io/",
    "logo_only": True,
    "version_selector": True,
    "prev_next_buttons_location": "both",
    "style_external_links": False,
    "style_nav_header_background": "#353130",
    # Toc options
    "collapse_navigation": True,
    "sticky_navigation": True,
    # 'navigation_depth': 4,
    "includehidden": False,
    "titles_only": False,
}

# Display "Edit on GitHub" link on each page
# https://docs.readthedocs.io/en/stable/guides/edit-source-links-sphinx.html
html_context = {
    "display_github": True,
    "theme_vcs_pageview_mode": "edit",
    "github_user": "rouault",
    "github_repo": "gdal_cli_workshop",
    "github_version": "master",
    "conf_py_path": "/src/",
}

html_static_path = ["_static"]

html_css_files = [
    'theme_overrides.css',
]

# Format used for 'Last updated on:' inserted at every page bottom
html_last_updated_fmt = today_fmt

# If true, links to the reST sources are added to the pages
html_show_sourcelink = False

# %% Options for LaTeX output

# The LaTeX engine to build the docs
latex_engine = "pdflatex"

# Grouping the document tree into LaTeX files. List of tuples
# (startdocname, targetname, title, author, theme, toctree_only)
latex_documents = [
    (root_doc, "gdal-cli-workshop.tex", title, author, "manual", False),
]

# Determines the topmost sectioning unit
latex_toplevel_sectioning = "chapter"

# See https://www.sphinx-doc.org/en/master/latex.html for options
latex_elements = {
    # The paper size ('letterpaper' or 'a4paper').
    # 'papersize': 'letterpaper',
    # The font size ('10pt', '11pt' or '12pt').
    # 'pointsize': '10pt',
    # Additional stuff for the LaTeX preamble
    "preamble": r"""
\ifdefined\DeclareUnicodeCharacter
  \DeclareUnicodeCharacter{2032}{$'$}% prime
\fi

\usepackage[notbib]{tocbibind}
""",
    "inputenc": r"\usepackage[utf8]{inputenc}",
    "makeindex": r"\usepackage[columns=1]{idxlayout}\makeindex",
    # Latex figure (float) alignment
    # 'figure_align': 'htbp',
}

# -- Source file links ------------------------------------------

#source_file_root = os.path.join(os.path.dirname(__file__), os.pardir, os.pardir)
#source_file_url_template = "https://github.com/OSGeo/PROJ/blob/master/{}"

# -- Specifics when documentation is built on ReadTheDocs infra

# Cf https://about.readthedocs.com/blog/2024/07/addons-by-default

# Define the canonical URL if you are using a custom domain on Read the Docs
html_baseurl = os.environ.get("READTHEDOCS_CANONICAL_URL", "https://gdal-cli-workshop.github.io/")

# Tell Jinja2 templates the build is running on Read the Docs
if os.environ.get("READTHEDOCS", "") == "True":
    html_context["READTHEDOCS"] = True

# -- Spelling --------------------------------------------------

# Avoid running git
spelling_ignore_contributor_names = False

# spelling_word_list_filename = ["spelling_wordlist.txt"]
