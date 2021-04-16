# Minimal makefile for Sphinx documentation
#

# You can set these variables from the command line.
SPHINXOPTS    ?= --keep-going -j auto -W
SPHINXBUILD   = sphinx-build
SOURCEDIR     = source
BUILDDIR      = build

# Use O="-D enable_redirects=1" with "make html" to create redirects

# Put it first so that "make" without argument is like "make help".
help:
	@$(SPHINXBUILD) -M help "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O)

.PHONY: help Makefile clean generated_rst_files

# Catch-all target: route all unknown targets to Sphinx using the new
# "make mode" option.  $(O) is meant as a shortcut for $(SPHINXOPTS).
%: Makefile
	@$(SPHINXBUILD) -M $@ "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O)

.doxygen_up_to_date:
	mkdir -p build/xml && cd .. && ((cat Doxyfile | sed "s/PREDEFINED             = /PREDEFINED             = DOXYGEN_XML /"; printf "GENERATE_HTML=NO\nGENERATE_XML=YES\nXML_OUTPUT=doc/build/xml\nXML_PROGRAMLISTING=NO") | doxygen -)
	mkdir -p build/doxygen_html && cd .. && ((cat Doxyfile; printf "HTML_OUTPUT=doc/build/doxygen_html\nINLINE_INHERITED_MEMB=YES") | doxygen -)
	touch .doxygen_up_to_date

generated_rst_files: .doxygen_up_to_date
	python3 "$(SOURCEDIR)/build_driver_summary.py" "$(SOURCEDIR)/drivers/raster" raster_driver_summary "$(SOURCEDIR)/drivers/raster/driver_summary.rst"
	python3 "$(SOURCEDIR)/build_driver_summary.py" "$(SOURCEDIR)/drivers/vector" vector_driver_summary "$(SOURCEDIR)/drivers/vector/driver_summary.rst"
	python3 "$(SOURCEDIR)/build_configoptions_index.py"  "$(SOURCEDIR)" "$(SOURCEDIR)/user/configoptions_index_generated.rst"

.PHONY: html latexpdf
html: generated_rst_files
	$(SPHINXBUILD) -M html "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O)
	# Hack to hide the TOC
	sed 's/<div class="toctree-wrapper compound">/<div class="toctree-wrapper compound" hidden=1>/' < "$(BUILDDIR)/html/drivers/raster/index.html" > "$(BUILDDIR)/html/drivers/raster/index.html.mod"
	mv "$(BUILDDIR)/html/drivers/raster/index.html.mod" "$(BUILDDIR)/html/drivers/raster/index.html"
	sed 's/<div class="toctree-wrapper compound">/<div class="toctree-wrapper compound" hidden=1>/' < "$(BUILDDIR)/html/drivers/vector/index.html" > "$(BUILDDIR)/html/drivers/vector/index.html.mod"
	mv "$(BUILDDIR)/html/drivers/vector/index.html.mod" "$(BUILDDIR)/html/drivers/vector/index.html"
	sed "s/python_api_ref\.html/\.\.\/python\/index\.html/" < "$(BUILDDIR)/html/api/index.html" > "$(BUILDDIR)/html/api/index.html.mod"
	mv "$(BUILDDIR)/html/api/index.html.mod" "$(BUILDDIR)/html/api/index.html"
	sed 's/<div class="toctree-wrapper compound">/<div class="toctree-wrapper compound" hidden=1>/' < "$(BUILDDIR)/html/sponsors/index.html" > "$(BUILDDIR)/html/sponsors/index.html.mod"
	mv "$(BUILDDIR)/html/sponsors/index.html.mod" "$(BUILDDIR)/html/sponsors/index.html"
	rm -rf build/html/doxygen
	cp -r build/doxygen_html build/html/doxygen
	ln -sf ../latex/gdal.pdf build/html
	cp -f "source/sponsors/Sustainable GDAL Sponsorship Prospectus.pdf" build/html/sponsors

man: generated_rst_files
	$(SPHINXBUILD) -M man "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O)

latexpdf: generated_rst_files
	$(SPHINXBUILD) -M latexpdf "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O)

clean:
	rm -rf build/xml
	rm -rf build/doxygen_html
	rm -rf .doxygen_up_to_date
	@$(SPHINXBUILD) -M clean "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O)
