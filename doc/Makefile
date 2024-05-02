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
	$(MAKE) -B $(BUILDDIR)/.doxygen_up_to_date

# Target run by the CI code_checks.yml workflow to check that there are no Doxygen warnings
# Requires Doxygen >= 1.9.3
doxygen_check_warnings:
	@echo "Checking that Doxygen runs without warnings..."
	@$(MAKE) -B $(BUILDDIR)/.doxygen_up_to_date > /tmp/doxygen_gdal_log.txt 2>&1
	@if grep -i warning /tmp/doxygen_gdal_log.txt; then \
		echo "Doxygen warnings found!"; \
		echo "---------"; \
		echo "Full log:"; \
		echo "---------"; \
		cat /tmp/doxygen_gdal_log.txt; \
		echo ""; \
		echo "--------------------"; \
		echo "Extract of warnings:"; \
		echo "--------------------"; \
		grep -i warning /tmp/doxygen_gdal_log.txt; \
		echo "-----------------------"; \
		echo "Doxygen warnings found!"; \
		/bin/false; \
	else \
		echo "No Doxygen warnings found"; \
	fi

$(BUILDDIR)/.doxygen_up_to_date:
	@set -e ; \
	case $(BUILDDIR) in \
	    "/"*) \
	        BUILDDIR_ABS="$(BUILDDIR)"; \
	        ;; \
	    *) \
	        BUILDDIR_ABS="${PWD}/$(BUILDDIR)" \
	        ;; \
	esac; \
	rm -rf $(BUILDDIR)/xml; \
	mkdir -p $(BUILDDIR)/xml; \
	(cd .. && ((cat Doxyfile | sed "s/PREDEFINED             = /PREDEFINED             = DOXYGEN_XML /"; printf "GENERATE_HTML=NO\nGENERATE_XML=YES\nXML_OUTPUT=$$BUILDDIR_ABS/xml\nXML_PROGRAMLISTING=NO") | doxygen -)); \
	rm -rf $(BUILDDIR)/doxygen_html; \
	mkdir -p $(BUILDDIR)/doxygen_html; \
	(cd .. && ((cat Doxyfile; printf "HTML_OUTPUT=$$BUILDDIR_ABS/doxygen_html\nINLINE_INHERITED_MEMB=YES") | doxygen -)); \
	echo "Doxygen replaces -- with <ndash/>. This is not desirable, so revert that;"; \
	for i in $(BUILDDIR)/xml/*.xml; do sed "s/<ndash\/>/--/g" < $$i > $$i.tmp; mv $$i.tmp $$i; done; \
	touch $(BUILDDIR)/.doxygen_up_to_date

generated_rst_files: $(BUILDDIR)/.doxygen_up_to_date
	$(PYTHON) "$(SOURCEDIR)/build_driver_summary.py" "$(SOURCEDIR)/drivers/raster" raster_driver_summary "$(SOURCEDIR)/drivers/raster/driver_summary.rst"
	$(PYTHON) "$(SOURCEDIR)/build_driver_summary.py" "$(SOURCEDIR)/drivers/vector" vector_driver_summary "$(SOURCEDIR)/drivers/vector/driver_summary.rst"

.PHONY: html latexpdf
html: generated_rst_files
	$(SPHINXBUILD) -M html "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O)
	# Hack to hide the TOC
	sed 's/<div class="toctree-wrapper compound">/<div class="toctree-wrapper compound" style="display: none;">/' < "$(BUILDDIR)/html/drivers/raster/index.html" > "$(BUILDDIR)/html/drivers/raster/index.html.mod"
	mv "$(BUILDDIR)/html/drivers/raster/index.html.mod" "$(BUILDDIR)/html/drivers/raster/index.html"
	sed 's/<div class="toctree-wrapper compound">/<div class="toctree-wrapper compound" style="display: none;">/' < "$(BUILDDIR)/html/drivers/vector/index.html" > "$(BUILDDIR)/html/drivers/vector/index.html.mod"
	mv "$(BUILDDIR)/html/drivers/vector/index.html.mod" "$(BUILDDIR)/html/drivers/vector/index.html"
	sed "s/python_api_ref\.html/\.\.\/python\/index\.html/" < "$(BUILDDIR)/html/api/index.html" > "$(BUILDDIR)/html/api/index.html.mod"
	mv "$(BUILDDIR)/html/api/index.html.mod" "$(BUILDDIR)/html/api/index.html"
	sed 's/<div class="toctree-wrapper compound">/<div class="toctree-wrapper compound" style="display: none;">/' < "$(BUILDDIR)/html/sponsors/index.html" > "$(BUILDDIR)/html/sponsors/index.html.mod"
	mv "$(BUILDDIR)/html/sponsors/index.html.mod" "$(BUILDDIR)/html/sponsors/index.html"
	rm -rf $(BUILDDIR)/html/doxygen
	cp -r $(BUILDDIR)/doxygen_html $(BUILDDIR)/html/doxygen
	ln -sf ../latex/gdal.pdf $(BUILDDIR)/html
	cp -f "source/sponsors/Sustainable GDAL Sponsorship Prospectus.pdf" $(BUILDDIR)/html/sponsors

man: generated_rst_files
	$(SPHINXBUILD) -M man "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O)

latexpdf: generated_rst_files
	$(SPHINXBUILD) -M latexpdf "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O)

clean:
	rm -rf "$(BUILDDIR)/xml"
	rm -rf "$(BUILDDIR)/doxygen_html"
	rm -f "$(BUILDDIR)/.doxygen_up_to_date"
	@$(SPHINXBUILD) -M clean "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O)
