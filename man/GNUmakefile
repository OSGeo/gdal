
MAN1_FILES	=	$(wildcard man1/*.1)

include ../GDALmake.opt

install:
	$(INSTALL_DIR) $(INST_MAN)/man1
	for f in $(MAN1_FILES) ; do $(INSTALL_DATA) $$f $(INST_MAN)/man1 ; done
