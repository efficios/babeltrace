all-local: doxygen-doc

install-data-local: doxygen-doc
	cp -rv output/html "$(DESTDIR)$(docdir)/api"

@DX_RULES@

MOSTLYCLEANFILES = $(DX_CLEANFILES)
EXTRA_DIST = Doxyfile.in \
	dox/group-api-ref.dox \
	dox/group-ctf-ir.dox \
	dox/main-page.dox \
	dox/quick-start.dox
