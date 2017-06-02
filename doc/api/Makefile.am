API_DOC_INSTALL_DIR = "$(DESTDIR)$(docdir)/api"

all-local: doxygen-doc

install-data-local: doxygen-doc
	$(mkdir_p) "$(API_DOC_INSTALL_DIR)"
	cp -rv output/html "$(API_DOC_INSTALL_DIR)"

@DX_RULES@

MOSTLYCLEANFILES = $(DX_CLEANFILES)
EXTRA_DIST = Doxyfile.in \
	README.adoc \
	dox/group-api-ref.dox \
	dox/group-ctf-ir.dox \
	dox/main-page.dox \
	dox/quick-start.dox
