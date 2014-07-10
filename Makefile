BUILD_SUBDIRS = threads userprog vm filesys

all: tags
	for d in $(BUILD_SUBDIRS); do $(MAKE) -C $$d $@; done

CLEAN_SUBDIRS = $(BUILD_SUBDIRS) examples utils

clean::
	for d in $(CLEAN_SUBDIRS); do $(MAKE) -C $$d $@; done
	rm -f TAGS tags

tags::
	ctags --recurse=yes .

distclean:: clean
	find . -name '*~' -exec rm '{}' \;

TAGS_SUBDIRS = $(BUILD_SUBDIRS) devices lib
TAGS_SOURCES = find $(TAGS_SUBDIRS) -name \*.[chS] -print

cscope.files::
	$(TAGS_SOURCES) > cscope.files
