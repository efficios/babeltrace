EXTRA_DIST = source

SPHINX_HTML_TARGET = build/html/index.html
SPHINX_DIRHTML_TARGET = build/dirhtml/index.html

all-local: $(SPHINX_HTML_TARGET) $(SPHINX_DIRHTML_TARGET)

PP = $(top_srcdir)/doc/bindings/python:$(top_srcdir)/doc/bindings/python/swig-libs
LLP = $(top_builddir)/formats/ctf/.libs:$(top_builddir)/lib/.libs

LOCAL_SPHINXBUILD = PYTHONPATH="$(PP)" LD_LIBRARY_PATH="$(LLP)" $(SPHINXBUILD)

SPHINX_SRC = $(wildcard source/*.rst)

$(SPHINX_HTML_TARGET): $(SPHINX_SRC) babeltrace-python-deps
	$(LOCAL_SPHINXBUILD) -b dirhtml -E source build/dirhtml

$(SPHINX_DIRHTML_TARGET): $(SPHINX_SRC) babeltrace-python-deps
	$(LOCAL_SPHINXBUILD) -b html -E source build/html

.PHONY: babeltrace-python-deps

babeltrace-python-deps: babeltrace swig-libs

swig-libs:
	$(LN_S) $(top_builddir)/bindings/python/.libs $@

babeltrace:
	$(LN_S) $(top_srcdir)/bindings/python $@

clean-local:
	rm -rf build swig-libs babeltrace
