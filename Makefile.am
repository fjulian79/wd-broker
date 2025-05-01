SUBDIRS = src

dist_testdir = wd-broker-test

test-package: all
	@echo "Preparing test package..."
	mkdir -p $(dist_testdir)
	rm -rf $(dist_testdir)/*
	cp src/wd-broker $(dist_testdir)/
	cp src/wd-ctrl $(dist_testdir)/
	cp test/common.py $(dist_testdir)/
	cp test/test-*.py $(dist_testdir)/
	cp test/run-tests.sh $(dist_testdir)/
	tar czf wd-broker-test.tar.gz $(dist_testdir)
	@echo "Test package created: wd-broker-test.tar.gz"
