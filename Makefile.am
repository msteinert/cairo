SUBDIRS = src test

EXTRA_DIST = \
	COPYING \
	COPYING-LGPL-2.1 \
	COPYING-MPL-1.1 \
	cairo.pc.in

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = cairo.pc

# Some custom targets to make it easier to release things.
# Use either:
#		make release-check
# or		make release-publish

RELEASE_UPLOAD_DIR = 	cairographics.org:/home/www/cairo/snapshots
RELEASE_URL_BASE = 	http://cairographics.org/snapshots
RELEASE_ANNOUNCE_LIST = cairo-announce@cairographics.org

tar_file = $(PACKAGE)-$(VERSION).tar.gz
md5_file = $(tar_file).md5

$(md5_file): $(tar_file)
	md5sum $^ > $@

release-remove-old:
	rm -f $(tar_file) $(md5_file)

release-check: release-remove-old distcheck $(md5_file)

release-publish: release-check
	mkdir -p releases
	scp $(tar_file) $(md5_file) $(RELEASE_UPLOAD_DIR)
	mv $(tar_file) $(md5_file) releases
	@echo ""
	@echo "Please send an announcement to $(RELEASE_ANNOUNCE_LIST)"
	@echo "including the following:"
	@echo ""
	@echo "Subject: $(PACKAGE) snapshot $(VERSION) now available"
	@echo ""
	@echo "A new $(PACKAGE) snapshot $(VERSION) is now available from:"
	@echo ""
	@echo "	$(RELEASE_URL_BASE)/$(tar_file)"
	@echo "	$(RELEASE_URL_BASE)/$(md5_file)"
	@echo -n "	"
	@cat releases/$(md5_file)
	@echo ""
	@echo "Also, please include the new entries from the NEWS file."

