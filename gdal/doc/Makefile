# Minimal makefile for Sphinx documentation
#

# You can set these variables from the command line.
SPHINXOPTS    = --keep-going -j auto -W
SPHINXBUILD   = sphinx-build
SOURCEDIR     = source
BUILDDIR      = build

# Put it first so that "make" without argument is like "make help".
help:
	@$(SPHINXBUILD) -M help "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O)

.PHONY: help Makefile clean

# Catch-all target: route all unknown targets to Sphinx using the new
# "make mode" option.  $(O) is meant as a shortcut for $(SPHINXOPTS).
%: Makefile
	@$(SPHINXBUILD) -M $@ "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O)

.doxygen_up_to_date:
	mkdir -p build/xml && cd .. && ((cat Doxyfile | sed "s/PREDEFINED             = /PREDEFINED             = DOXYGEN_XML /"; printf "GENERATE_HTML=NO\nGENERATE_XML=YES\nXML_OUTPUT=doc/build/xml\nXML_PROGRAMLISTING=NO") | doxygen -)
	mkdir -p build/doxygen_html && cd .. && ((cat Doxyfile; printf "HTML_OUTPUT=doc/build/doxygen_html") | doxygen -)
	touch .doxygen_up_to_date

.PHONY: html latexpdf
html: .doxygen_up_to_date
	$(SPHINXBUILD) -M html "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O)
	rm -rf build/html/doxygen
	cp -r build/doxygen_html build/html/doxygen
	ln -sf ../latex/gdal.pdf build/html

man: .doxygen_up_to_date
	$(SPHINXBUILD) -M man "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O)

latexpdf: .doxygen_up_to_date
	$(SPHINXBUILD) -M latexpdf "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O)

clean:
	rm -rf build/xml
	rm -rf build/doxygen_html
	rm -rf .doxygen_up_to_date
	@$(SPHINXBUILD) -M clean "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O)
