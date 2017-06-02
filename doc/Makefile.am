SUBDIRS = bindings images

if ENABLE_API_DOC
SUBDIRS += api
endif

dist_man_MANS = babeltrace.1 babeltrace-log.1

EXTRA_DIST = development.txt logging-guide.adoc ref-counting.md API.txt lttng-live.txt

logging-guide.html: logging-guide.adoc
	asciidoc --verbose -a source-highlighter=pygments logging-guide.adoc

CLEANFILES = logging-guide.html
