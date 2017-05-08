SUBDIRS = bindings images

if ENABLE_API_DOC
SUBDIRS += api
endif

dist_man_MANS = babeltrace.1 babeltrace-log.1

dist_doc_DATA = API.txt lttng-live.txt ref-counting.md logging-guide.adoc

EXTRA_DIST = development.txt

logging-guide.html: logging-guide.adoc
	asciidoc --verbose -a source-highlighter=pygments logging-guide.adoc

CLEANFILES = logging-guide.html
