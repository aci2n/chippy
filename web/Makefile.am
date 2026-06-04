AM_CPPFLAGS = -I$(srcdir)/include -DWEB_STATIC_DIR=\"$(abs_srcdir)/static\"

bin_PROGRAMS = web
web_SOURCES = \
	src/main.c \
	src/server.c

check-local:
	@$(srcdir)/tests/test_web.sh "$(top_builddir)/web/web"

EXTRA_DIST = static/index.html static/style.css static/app.js tests/test_web.sh
