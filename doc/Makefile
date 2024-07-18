# Minimal makefile for Sphinx documentation
#

# You can set these variables from the command line.
PYTHON        ?= python3
SPHINXOPTS    ?= --keep-going -j auto -W
SPHINXBUILD   ?= sphinx-build
SOURCEDIR     = source
BUILDDIR      = build

# Use O="-D enable_redirects=1" with "make html" to create redirects

# Put it first so that "make" without argument is like "make help".
help:
	@$(SPHINXBUILD) -M help "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O)

.PHONY: help Makefile clean generated_rst_files doxygen

# Catch-all target: route all unknown targets to Sphinx using the new
# "make mode" option.  $(O) is meant as a shortcut for $(SPHINXOPTS).
%: Makefile
	@$(SPHINXBUILD) -M $@ "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O)

doxygen:
	$(MAKE) -B .doxygen_up_to_date

.doxygen_up_to_date:
	mkdir -p build/xml && cd .. && ((cat Doxyfile | sed "s/PREDEFINED             = /PREDEFINED             = DOXYGEN_XML /"; printf "GENERATE_HTML=NO\nGENERATE_XML=YES\nXML_OUTPUT=doc/build/xml\nXML_PROGRAMLISTING=NO") | doxygen -)
	mkdir -p build/html_extra/doxygen && cd .. && ((cat Doxyfile; printf "HTML_OUTPUT=doc/build/html_extra/doxygen\nINLINE_INHERITED_MEMB=YES") | doxygen -)
	# Doxygen replaces -- with <ndash/>. This is not desirable, so revert that
	for i in build/xml/*.xml; do sed "s/<ndash\/>/--/g" < $$i > $$i.tmp; mv $$i.tmp $$i; done
	touch .doxygen_up_to_date

generated_rst_files: .doxygen_up_to_date
	$(PYTHON) "$(SOURCEDIR)/build_driver_summary.py" "$(SOURCEDIR)/drivers/raster" raster_driver_summary "$(SOURCEDIR)/drivers/raster/driver_summary.rst"
	$(PYTHON) "$(SOURCEDIR)/build_driver_summary.py" "$(SOURCEDIR)/drivers/vector" vector_driver_summary "$(SOURCEDIR)/drivers/vector/driver_summary.rst"

.PHONY: html latexpdf
html: generated_rst_files
	$(SPHINXBUILD) -M html "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O)
	ln -sf ../latex/gdal.pdf build/html

man: generated_rst_files
	$(SPHINXBUILD) -M man "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O)

latexpdf: generated_rst_files
	$(SPHINXBUILD) -M latexpdf "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O)

clean:
	rm -rf build/xml
	rm -rf build/html_extra/doxygen
	rm -rf .doxygen_up_to_date
	@$(SPHINXBUILD) -M clean "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O)
