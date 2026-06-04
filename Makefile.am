# recurse into subprojects when top-level make runs
SUBDIRS = core

# Reconfigure with ASan/UBSan and run the test suite (from a build dir).
.PHONY: check-asan
check-asan:
	@$(abs_top_srcdir)/autogen.sh
	./configure --enable-sanitize
	@$(MAKE) check
