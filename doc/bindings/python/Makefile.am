# SPDX-License-Identifier: MIT

SPHINX_SOURCE_DIR = $(srcdir)/source
SPHINX_EXT_DIR = $(srcdir)/ext
SPHINX_BUILD_DIR = $(builddir)/build
SPHINX_BUILD_HTML_DIR = $(SPHINX_BUILD_DIR)/html
SPHINX_HTML_TARGET = $(SPHINX_BUILD_HTML_DIR)/index.html

EXTRA_DIST = $(SPHINX_SOURCE_DIR) $(SPHINX_EXT_DIR)

all-local: $(SPHINX_HTML_TARGET)

PYTHON_BT2_BUILD_LIB_DIR = $(abs_top_builddir)/src/bindings/python/bt2/build/build_lib
PP = $(PYTHON_BT2_BUILD_LIB_DIR)
LLP = $(abs_top_builddir)/src/lib/.libs

# Sphinx loads the bt2 Python module and thus libbabeltrace2.so.  If
# AddressSanitizer is used, we must preload libasan.so so that libasan doesn't
# complain about not being the first loaded library.
#
# Python produces some leaks, so disable leak detection (we don't care about
# leaks here anyway).
if ENABLE_ASAN
MAYBE_LD_PRELOAD = LD_PRELOAD="$$($(CC) -print-file-name=libasan.so):$(LD_PRELOAD)"
MAYBE_ASAN_OPTIONS = ASAN_OPTIONS="$(ASAN_OPTIONS),detect_leaks=0"
endif

# `PATH` is used as a replacement for `LD_LIBRARY_PATH` on Windows
# builds (Cygwin, MinGW).
#
# `DYLD_LIBRARY_PATH` is used a replacement for `LD_LIBRARY_PATH` on
# macOS builds.
SPHINXBUILD = \
	PATH="$(LLP):$$PATH" \
	PYTHONPATH="$(PP):$(SPHINX_EXT_DIR)" \
	LD_LIBRARY_PATH="$(LLP)" \
	DYLD_LIBRARY_PATH="$(LLP)" \
	$(MAYBE_LD_PRELOAD) \
	$(MAYBE_ASAN_OPTIONS) \
	$(PYTHON) -m sphinx
SPHINX_SRC = \
	$(SPHINX_SOURCE_DIR)/common.rst \
	$(SPHINX_SOURCE_DIR)/index.rst \
	$(SPHINX_SOURCE_DIR)/installation.rst \
	$(SPHINX_SOURCE_DIR)/examples.rst

$(SPHINX_HTML_TARGET): $(SPHINX_SRC)
	$(SPHINXBUILD) -b html -E $(SPHINX_SOURCE_DIR) $(SPHINX_BUILD_HTML_DIR)

clean-local:
	rm -rf $(SPHINX_BUILD_DIR)
