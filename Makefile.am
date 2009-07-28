SUBDIRS :=
ACLOCAL_AMFLAGS =-I m4
# Extra libraries should be compiled first
if BUNDLED_SQLITE
SUBDIRS += extlibs/sqlite-3.6.6.2 
endif

if OS_IPHONEOS
SUBDIRS += $(OPENSSL_DIR)
endif

SUBDIRS += doc src

EXTRA_DIST = bin lib extlibs winmobile windows iphone android macosx README Android.mk
DIST_SUBDIRS = $(SUBDIRS) testsuite

dist_doc_DATA = README
docdir = doc

if DOCS
EXTRA_DIST += Doxygen.in 
all-local: Doxyfile
docs:
	@echo "Generating documentation"
	doxygen Doxyfile
endif

tes%:
	$(MAKE) -C testsuite $@

clean-local:
	rm -f *~
	rm -f bin/*
	rm -f lib/*

realclean:
	svn status --no-ignore | grep "I  " | awk '{print $2}' | xargs rm -rf

dist-hook:
        # Remove all the .svn directories that may have made it into the distribution
	rm -rf `find $(distdir)/ -name .svn`
	rm -rf `find $(distdir)/extlibs -name .deps`
	rm -rf $(distdir)/bin/*
	rm -rf $(distdir)/lib/*

distcheck-hook:
	@echo "Distcheck hook! $(PWD)"
	mkdir -p $(distdir)/_build/bin
	mkdir -p $(distdir)/_build/lib
