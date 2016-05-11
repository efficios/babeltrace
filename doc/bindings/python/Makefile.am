SPHINX_SOURCE_DIR = $(srcdir)/source
SPHINX_BUILD_DIR = build
SPHINX_BUILD_HTML_DIR = $(SPHINX_BUILD_DIR)/html
SPHINX_BUILD_DIRHTML_DIR = $(SPHINX_BUILD_DIR)/dirhtml

SPHINX_HTML_TARGET = $(SPHINX_BUILD_HTML_DIR)/index.html
SPHINX_DIRHTML_TARGET = $(SPHINX_BUILD_DIRHTML_DIR)/index.html

EXTRA_DIST = $(srcdir)/source

all-local: $(SPHINX_HTML_TARGET) $(SPHINX_DIRHTML_TARGET)

PP = .:swig-libs
LLP = $(top_builddir)/formats/ctf/.libs:$(top_builddir)/lib/.libs

LOCAL_SPHINXBUILD = PYTHONPATH="$(PP)" LD_LIBRARY_PATH="$(LLP)" $(PYTHON) $(srcdir)/sphinx-build.py

SPHINX_SRC = $(wildcard $(SPHINX_SOURCE_DIR)/*.rst)

BABELTRACE_PYTHON_DEPS = babeltrace swig-libs

$(SPHINX_HTML_TARGET): $(SPHINX_SRC) $(BABELTRACE_PYTHON_DEPS)
	$(LOCAL_SPHINXBUILD) -b html -E $(SPHINX_SOURCE_DIR) $(SPHINX_BUILD_HTML_DIR)

$(SPHINX_DIRHTML_TARGET): $(SPHINX_SRC) $(BABELTRACE_PYTHON_DEPS)
	$(LOCAL_SPHINXBUILD) -b dirhtml -E $(SPHINX_SOURCE_DIR) $(SPHINX_BUILD_DIRHTML_DIR)

swig-libs:
	$(LN_S) $(abs_top_builddir)/bindings/python/babeltrace/.libs $@

babeltrace:
	$(mkdir_p) $@
	$(LN_S) $(abs_top_builddir)/bindings/python/babeltrace/__init__.py $@/__init__.py
	$(LN_S) $(abs_top_srcdir)/bindings/python/babeltrace/common.py $@/common.py
	$(LN_S) $(abs_top_srcdir)/bindings/python/babeltrace/reader.py $@/reader.py
	$(LN_S) $(abs_top_srcdir)/bindings/python/babeltrace/writer.py $@/writer.py
	$(LN_S) $(abs_top_builddir)/bindings/python/babeltrace/nativebt.py $@/nativebt.py

clean-local:
	rm -rf $(SPHINX_BUILD_DIR) swig-libs babeltrace
