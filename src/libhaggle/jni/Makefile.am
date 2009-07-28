SUBDIRS = java/org/haggle native
JARFILE = haggle.jar
MANIFEST = Manifest.txt
haggledir = $(datadir)/java
haggle_DATA = $(JARFILE)
EXTRA_DIST = $(MANIFEST) README Android.mk
CLASSFILES = $(shell find java -name *.class -print)

classes.list: $(SUBDIRS) $(CLASSFILES)
	@echo $(CLASSFILES)
	(cd java; find . -name "*.class" -print > ../$@)

$(JARFILE): classes.list $(MANIFEST)
	@echo "Creating Jar file"
	(cd java; jar cfm ../$@ ../$(MANIFEST) @../$<)
	cp -f $@ $(top_srcdir)/lib/

clean-local:
	rm -f *~
	rm -f $(JARFILE)
	rm -f classes.list
