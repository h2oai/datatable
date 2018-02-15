BUILDDIR := build/fast
PYTHON ?= python
OS := $(shell uname | tr A-Z a-z)
MODULE ?= .


.PHONY: all clean mrproper build install uninstall test_install test \
		benchmark debug build_noomp bi coverage fast
.SECONDARY: main-fast


all:
	$(MAKE) clean
	$(MAKE) build
	$(MAKE) install
	$(MAKE) test


clean::
	rm -rf .cache
	rm -rf .eggs
	rm -rf build
	rm -rf dist
	rm -rf datatable.egg-info
	rm -f *.so
	rm -f datatable/lib/_datatable*.so
	find . -type d -name "__pycache__" -exec rm -rf {} +


mrproper: clean
	git clean -f -d -x


build:
	$(PYTHON) setup.py build
	cp build/lib.*/datatable/lib/_datatable.* datatable/lib/


install:
	$(PYTHON) -m pip install . --upgrade --no-cache-dir


uninstall:
	$(PYTHON) -m pip uninstall datatable -y


test_install:
	$(PYTHON) -m pip install ${MODULE}[testing] --no-cache-dir


test:
	$(MAKE) test_install
	rm -rf build/test-reports 2>/dev/null
	mkdir -p build/test-reports/
	$(PYTHON) -m pytest -ra \
		--junit-prefix=$(OS) \
		--junitxml=build/test-reports/TEST-datatable.xml \
		tests


benchmark:
	$(PYTHON) -m pytest -ra -v benchmarks


debug:
	$(MAKE) clean
	DTDEBUG=1 \
	$(MAKE) build
	$(MAKE) install

build_noomp:
	DTNOOPENMP=1 \
	$(MAKE) build


bi:
	$(MAKE) build
	$(MAKE) install


coverage:
	$(MAKE) clean
	DTCOVERAGE=1 \
	$(MAKE) build
	$(MAKE) install
	$(MAKE) test_install
	$(PYTHON) -m pytest \
		--benchmark-skip \
		--cov=datatable --cov-report=html:build/coverage-py \
		tests
	$(PYTHON) -m pytest \
		--benchmark-skip \
		--cov=datatable --cov-report=xml:build/coverage.xml \
		tests
	chmod +x ./llvm-gcov.sh
	lcov --gcov-tool ./llvm-gcov.sh --capture --directory . --output-file build/coverage.info
	genhtml build/coverage.info --output-directory build/coverage-c
	mv .coverage build/



#-------------------------------------------------------------------------------
# "Fast" (but fragile) datatable build
#-------------------------------------------------------------------------------

fast_objects = $(addprefix $(BUILDDIR)/, \
	capi.o                    \
	column.o                  \
	column_bool.o             \
	column_from_pylist.o      \
	column_fw.o               \
	column_int.o              \
	column_pyobj.o            \
	column_real.o             \
	column_string.o           \
	columnset.o               \
	csv/fread.o               \
	csv/py_csv.o              \
	csv/reader.o              \
	csv/reader_arff.o         \
	csv/reader_fread.o        \
	csv/reader_parsers.o      \
	csv/reader_utils.o        \
	csv/writer.o              \
	datatable.o               \
	datatable_cbind.o         \
	datatable_check.o         \
	datatable_load.o          \
	datatable_rbind.o         \
	datatablemodule.o         \
	encodings.o               \
	expr/binaryop.o           \
	expr/py_expr.o            \
	expr/reduceop.o           \
	expr/unaryop.o            \
	memorybuf.o               \
	mmm.o                     \
	py_buffers.o              \
	py_column.o               \
	py_columnset.o            \
	py_datatable.o            \
	py_datatable_fromlist.o   \
	py_datawindow.o           \
	py_encodings.o            \
	py_rowindex.o             \
	py_types.o                \
	py_utils.o                \
	python/list.o             \
	rowindex.o                \
	rowindex_array.o          \
	rowindex_slice.o          \
	sort.o                    \
	stats.o                   \
	types.o                   \
	utils.o                   \
	utils/exceptions.o        \
	utils/file.o              \
	utils/pyobj.o             \
	writebuf.o                \
	)

fast:
	$(eval CC := $(shell python setup.py get_CC))
	$(eval CCFLAGS := $(shell python setup.py get_CCFLAGS))
	$(eval LDFLAGS := $(shell python setup.py get_LDFLAGS))
	$(eval EXTEXT := $(shell python setup.py get_EXTEXT))
	$(eval export CC CCFLAGS LDFLAGS EXTEXT)
	@echo • Checking dependencies graph
	@python fastcheck.py
	@$(MAKE) --no-print-directory main-fast

post-fast:
	@echo • Copying _datatable.so into ``datatable/lib/_datatable$(EXTEXT)``
	@cp $(BUILDDIR)/_datatable.so datatable/lib/_datatable$(EXTEXT)

main-fast: $(BUILDDIR)/_datatable.so
	@echo • Done.

# ------------------------------------------------------------
#
# New targets used in Jenkinsfile for DAI datatable build
#    mrproper
#    centos7_in_docker
#

DIST_DIR = dist

ARCH := $(shell arch)
PLATFORM := $(ARCH)-centos7

CONTAINER_NAME_SUFFIX ?= -$(USER)
CONTAINER_NAME ?= opsh2oai/dai-datatable$(CONTAINER_NAME_SUFFIX)

PROJECT_VERSION := $(shell grep '^version' datatable/__version__.py | sed 's/version = //' | sed 's/\"//g')
BRANCH_NAME ?= $(shell git rev-parse --abbrev-ref HEAD)
BRANCH_NAME_SUFFIX = -$(BRANCH_NAME)
BUILD_NUM ?= local
BUILD_NUM_SUFFIX = -$(BUILD_NUM)
CONTAINER_TAG = $(PROJECT_VERSION)$(BRANCH_NAME_SUFFIX)$(BUILD_NUM_SUFFIX)

CONTAINER_NAME_TAG = $(CONTAINER_NAME):$(CONTAINER_TAG)

ARCH_SUBST = undefined
FROM_SUBST = undefined
ifeq ($(ARCH),x86_64)
    FROM_SUBST = centos:7
    ARCH_SUBST = $(ARCH)
endif
ifeq ($(ARCH),ppc64le)
    FROM_SUBST = ibmcom\/centos-ppc64le
    ARCH_SUBST = $(ARCH)
endif

Dockerfile-centos7.$(PLATFORM): Dockerfile-centos7.in
	cat $< | sed 's/FROM_SUBST/$(FROM_SUBST)/'g | sed 's/ARCH_SUBST/$(ARCH_SUBST)/g' > $@

centos7_in_docker: Dockerfile-centos7.$(PLATFORM)
	mkdir -p $(DIST_DIR)/$(PLATFORM)
	docker build \
		-t $(CONTAINER_NAME_TAG) \
		-f Dockerfile-centos7.$(PLATFORM) \
		.
	docker run \
		--rm \
		--init \
		-v `pwd`:/dot \
		-w /dot \
		--entrypoint /bin/bash \
		$(CONTAINER_NAME_TAG) \
		-c 'python3.6 setup.py bdist_wheel'
	echo $(CONTAINER_TAG) > $(DIST_DIR)/$(PLATFORM)/VERSION.txt

printvars:
	@echo $(PLATFORM)
	@echo $(PROJECT_VERSION)

clean::
	rm -f Dockerfile-centos7.$(PLATFORM)

# ------------------------------------------------------------

$(BUILDDIR)/_datatable.so: $(fast_objects)
	@echo • Linking object files into _datatable.so
	@$(CC) $(LDFLAGS) -o $@ $+
	@$(MAKE) --no-print-directory post-fast



$(BUILDDIR)/datatable.h: c/datatable.h $(BUILDDIR)/column.h $(BUILDDIR)/datatable_check.h $(BUILDDIR)/rowindex.h $(BUILDDIR)/types.h
	@echo • Refreshing c/datatable.h
	@cp c/datatable.h $@

$(BUILDDIR)/utils.h: c/utils.h $(BUILDDIR)/utils/exceptions.h
	@echo • Refreshing c/utils.h
	@cp c/utils.h $@

$(BUILDDIR)/types.h: c/types.h
	@echo • Refreshing c/types.h
	@cp c/types.h $@

$(BUILDDIR)/mmm.h: c/mmm.h
	@echo • Refreshing c/mmm.h
	@cp c/mmm.h $@

$(BUILDDIR)/py_datawindow.h: c/py_datawindow.h
	@echo • Refreshing c/py_datawindow.h
	@cp c/py_datawindow.h $@

$(BUILDDIR)/rowindex.h: c/rowindex.h $(BUILDDIR)/utils/array.h
	@echo • Refreshing c/rowindex.h
	@cp c/rowindex.h $@

$(BUILDDIR)/py_rowindex.h: c/py_rowindex.h $(BUILDDIR)/py_utils.h $(BUILDDIR)/rowindex.h
	@echo • Refreshing c/py_rowindex.h
	@cp c/py_rowindex.h $@

$(BUILDDIR)/encodings.h: c/encodings.h
	@echo • Refreshing c/encodings.h
	@cp c/encodings.h $@

$(BUILDDIR)/columnset.h: c/columnset.h $(BUILDDIR)/column.h $(BUILDDIR)/datatable.h $(BUILDDIR)/rowindex.h $(BUILDDIR)/types.h
	@echo • Refreshing c/columnset.h
	@cp c/columnset.h $@

$(BUILDDIR)/py_column.h: c/py_column.h $(BUILDDIR)/column.h $(BUILDDIR)/py_datatable.h $(BUILDDIR)/py_utils.h
	@echo • Refreshing c/py_column.h
	@cp c/py_column.h $@

$(BUILDDIR)/py_types.h: c/py_types.h $(BUILDDIR)/datatable.h $(BUILDDIR)/types.h $(BUILDDIR)/utils/assert.h
	@echo • Refreshing c/py_types.h
	@cp c/py_types.h $@

$(BUILDDIR)/column.h: c/column.h $(BUILDDIR)/memorybuf.h $(BUILDDIR)/rowindex.h $(BUILDDIR)/stats.h $(BUILDDIR)/types.h
	@echo • Refreshing c/column.h
	@cp c/column.h $@

$(BUILDDIR)/py_utils.h: c/py_utils.h $(BUILDDIR)/utils.h $(BUILDDIR)/utils/exceptions.h
	@echo • Refreshing c/py_utils.h
	@cp c/py_utils.h $@

$(BUILDDIR)/writebuf.h: c/writebuf.h $(BUILDDIR)/utils/file.h
	@echo • Refreshing c/writebuf.h
	@cp c/writebuf.h $@

$(BUILDDIR)/capi.h: c/capi.h
	@echo • Refreshing c/capi.h
	@cp c/capi.h $@

$(BUILDDIR)/py_encodings.h: c/py_encodings.h $(BUILDDIR)/encodings.h
	@echo • Refreshing c/py_encodings.h
	@cp c/py_encodings.h $@

$(BUILDDIR)/py_columnset.h: c/py_columnset.h $(BUILDDIR)/datatable.h $(BUILDDIR)/py_utils.h $(BUILDDIR)/rowindex.h
	@echo • Refreshing c/py_columnset.h
	@cp c/py_columnset.h $@

$(BUILDDIR)/memorybuf.h: c/memorybuf.h $(BUILDDIR)/datatable_check.h $(BUILDDIR)/mmm.h $(BUILDDIR)/writebuf.h
	@echo • Refreshing c/memorybuf.h
	@cp c/memorybuf.h $@

$(BUILDDIR)/sort.h: c/sort.h
	@echo • Refreshing c/sort.h
	@cp c/sort.h $@

$(BUILDDIR)/py_datatable.h: c/py_datatable.h $(BUILDDIR)/datatable.h $(BUILDDIR)/py_utils.h
	@echo • Refreshing c/py_datatable.h
	@cp c/py_datatable.h $@

$(BUILDDIR)/stats.h: c/stats.h $(BUILDDIR)/datatable_check.h $(BUILDDIR)/types.h
	@echo • Refreshing c/stats.h
	@cp c/stats.h $@

$(BUILDDIR)/datatable_check.h: c/datatable_check.h
	@echo • Refreshing c/datatable_check.h
	@cp c/datatable_check.h $@

$(BUILDDIR)/expr/py_expr.h: c/expr/py_expr.h $(BUILDDIR)/column.h $(BUILDDIR)/py_utils.h
	@echo • Refreshing c/expr/py_expr.h
	@cp c/expr/py_expr.h $@

$(BUILDDIR)/python/list.h: c/python/list.h $(BUILDDIR)/utils/pyobj.h
	@echo • Refreshing c/python/list.h
	@cp c/python/list.h $@

$(BUILDDIR)/utils/file.h: c/utils/file.h
	@echo • Refreshing c/utils/file.h
	@cp c/utils/file.h $@

$(BUILDDIR)/utils/exceptions.h: c/utils/exceptions.h $(BUILDDIR)/types.h
	@echo • Refreshing c/utils/exceptions.h
	@cp c/utils/exceptions.h $@

$(BUILDDIR)/utils/array.h: c/utils/array.h $(BUILDDIR)/utils/exceptions.h
	@echo • Refreshing c/utils/array.h
	@cp c/utils/array.h $@

$(BUILDDIR)/utils/omp.h: c/utils/omp.h
	@echo • Refreshing c/utils/omp.h
	@cp c/utils/omp.h $@

$(BUILDDIR)/utils/pyobj.h: c/utils/pyobj.h
	@echo • Refreshing c/utils/pyobj.h
	@cp c/utils/pyobj.h $@

$(BUILDDIR)/utils/assert.h: c/utils/assert.h
	@echo • Refreshing c/utils/assert.h
	@cp c/utils/assert.h $@

$(BUILDDIR)/csv/reader.h: c/csv/reader.h $(BUILDDIR)/column.h $(BUILDDIR)/datatable.h $(BUILDDIR)/memorybuf.h $(BUILDDIR)/utils/pyobj.h $(BUILDDIR)/writebuf.h
	@echo • Refreshing c/csv/reader.h
	@cp c/csv/reader.h $@

$(BUILDDIR)/csv/reader_arff.h: c/csv/reader_arff.h $(BUILDDIR)/csv/reader.h
	@echo • Refreshing c/csv/reader_arff.h
	@cp c/csv/reader_arff.h $@

$(BUILDDIR)/csv/fread.h: c/csv/fread.h $(BUILDDIR)/csv/reader.h $(BUILDDIR)/memorybuf.h $(BUILDDIR)/utils.h $(BUILDDIR)/utils/omp.h
	@echo • Refreshing c/csv/fread.h
	@cp c/csv/fread.h $@

$(BUILDDIR)/csv/reader_parsers.h: c/csv/reader_parsers.h $(BUILDDIR)/csv/fread.h
	@echo • Refreshing c/csv/reader_parsers.h
	@cp c/csv/reader_parsers.h $@

$(BUILDDIR)/csv/itoa.h: c/csv/itoa.h
	@echo • Refreshing c/csv/itoa.h
	@cp c/csv/itoa.h $@

$(BUILDDIR)/csv/py_csv.h: c/csv/py_csv.h $(BUILDDIR)/py_utils.h
	@echo • Refreshing c/csv/py_csv.h
	@cp c/csv/py_csv.h $@

$(BUILDDIR)/csv/dtoa.h: c/csv/dtoa.h
	@echo • Refreshing c/csv/dtoa.h
	@cp c/csv/dtoa.h $@

$(BUILDDIR)/csv/writer.h: c/csv/writer.h $(BUILDDIR)/datatable.h $(BUILDDIR)/utils.h $(BUILDDIR)/writebuf.h
	@echo • Refreshing c/csv/writer.h
	@cp c/csv/writer.h $@

$(BUILDDIR)/csv/reader_fread.h: c/csv/reader_fread.h $(BUILDDIR)/csv/fread.h $(BUILDDIR)/csv/py_csv.h $(BUILDDIR)/csv/reader.h $(BUILDDIR)/memorybuf.h
	@echo • Refreshing c/csv/reader_fread.h
	@cp c/csv/reader_fread.h $@

$(BUILDDIR)/csv/freadLookups.h: c/csv/freadLookups.h
	@echo • Refreshing c/csv/freadLookups.h
	@cp c/csv/freadLookups.h $@



$(BUILDDIR)/capi.o : c/capi.cc $(BUILDDIR)/capi.h $(BUILDDIR)/datatable.h $(BUILDDIR)/rowindex.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/column.o : c/column.cc $(BUILDDIR)/column.h $(BUILDDIR)/datatable_check.h $(BUILDDIR)/py_utils.h $(BUILDDIR)/rowindex.h $(BUILDDIR)/sort.h $(BUILDDIR)/utils.h $(BUILDDIR)/utils/assert.h $(BUILDDIR)/utils/file.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/column_bool.o : c/column_bool.cc $(BUILDDIR)/column.h $(BUILDDIR)/utils/omp.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/column_from_pylist.o : c/column_from_pylist.cc $(BUILDDIR)/column.h $(BUILDDIR)/py_types.h $(BUILDDIR)/python/list.h $(BUILDDIR)/utils.h $(BUILDDIR)/utils/exceptions.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/column_fw.o : c/column_fw.cc $(BUILDDIR)/column.h $(BUILDDIR)/utils.h $(BUILDDIR)/utils/assert.h $(BUILDDIR)/utils/omp.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/column_int.o : c/column_int.cc $(BUILDDIR)/column.h $(BUILDDIR)/py_types.h $(BUILDDIR)/py_utils.h $(BUILDDIR)/utils/omp.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/column_pyobj.o : c/column_pyobj.cc $(BUILDDIR)/column.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/column_real.o : c/column_real.cc $(BUILDDIR)/column.h $(BUILDDIR)/py_utils.h $(BUILDDIR)/utils/omp.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/column_string.o : c/column_string.cc $(BUILDDIR)/column.h $(BUILDDIR)/datatable_check.h $(BUILDDIR)/encodings.h $(BUILDDIR)/py_utils.h $(BUILDDIR)/utils.h $(BUILDDIR)/utils/assert.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/columnset.o : c/columnset.cc $(BUILDDIR)/columnset.h $(BUILDDIR)/utils.h $(BUILDDIR)/utils/exceptions.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/csv/fread.o : c/csv/fread.cc $(BUILDDIR)/csv/fread.h $(BUILDDIR)/csv/freadLookups.h $(BUILDDIR)/csv/reader.h $(BUILDDIR)/csv/reader_fread.h $(BUILDDIR)/csv/reader_parsers.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/csv/py_csv.o : c/csv/py_csv.cc $(BUILDDIR)/csv/py_csv.h $(BUILDDIR)/csv/reader.h $(BUILDDIR)/csv/writer.h $(BUILDDIR)/py_datatable.h $(BUILDDIR)/py_utils.h $(BUILDDIR)/utils.h $(BUILDDIR)/utils/omp.h $(BUILDDIR)/utils/pyobj.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/csv/reader.o : c/csv/reader.cc $(BUILDDIR)/csv/reader.h $(BUILDDIR)/csv/reader_arff.h $(BUILDDIR)/csv/reader_fread.h $(BUILDDIR)/utils/exceptions.h $(BUILDDIR)/utils/omp.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/csv/reader_arff.o : c/csv/reader_arff.cc $(BUILDDIR)/csv/reader_arff.h $(BUILDDIR)/utils/exceptions.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/csv/reader_fread.o : c/csv/reader_fread.cc $(BUILDDIR)/column.h $(BUILDDIR)/csv/fread.h $(BUILDDIR)/csv/reader.h $(BUILDDIR)/csv/reader_fread.h $(BUILDDIR)/csv/reader_parsers.h $(BUILDDIR)/datatable.h $(BUILDDIR)/py_datatable.h $(BUILDDIR)/py_encodings.h $(BUILDDIR)/py_utils.h $(BUILDDIR)/utils.h $(BUILDDIR)/utils/assert.h $(BUILDDIR)/utils/file.h $(BUILDDIR)/utils/pyobj.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/csv/reader_parsers.o : c/csv/reader_parsers.cc $(BUILDDIR)/csv/reader_parsers.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/csv/reader_utils.o : c/csv/reader_utils.cc $(BUILDDIR)/csv/fread.h $(BUILDDIR)/csv/reader.h $(BUILDDIR)/utils/assert.h $(BUILDDIR)/utils/exceptions.h $(BUILDDIR)/utils/omp.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/csv/writer.o : c/csv/writer.cc $(BUILDDIR)/column.h $(BUILDDIR)/csv/dtoa.h $(BUILDDIR)/csv/itoa.h $(BUILDDIR)/csv/writer.h $(BUILDDIR)/datatable.h $(BUILDDIR)/memorybuf.h $(BUILDDIR)/types.h $(BUILDDIR)/utils.h $(BUILDDIR)/utils/omp.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/datatable.o : c/datatable.cc $(BUILDDIR)/datatable.h $(BUILDDIR)/datatable_check.h $(BUILDDIR)/py_utils.h $(BUILDDIR)/rowindex.h $(BUILDDIR)/types.h $(BUILDDIR)/utils/omp.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/datatable_cbind.o : c/datatable_cbind.cc $(BUILDDIR)/datatable.h $(BUILDDIR)/rowindex.h $(BUILDDIR)/utils.h $(BUILDDIR)/utils/assert.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/datatable_check.o : c/datatable_check.cc $(BUILDDIR)/datatable_check.h $(BUILDDIR)/utils.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/datatable_load.o : c/datatable_load.cc $(BUILDDIR)/datatable.h $(BUILDDIR)/utils.h $(BUILDDIR)/utils/assert.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/datatable_rbind.o : c/datatable_rbind.cc $(BUILDDIR)/column.h $(BUILDDIR)/datatable.h $(BUILDDIR)/utils.h $(BUILDDIR)/utils/assert.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/datatablemodule.o : c/datatablemodule.c $(BUILDDIR)/capi.h $(BUILDDIR)/csv/py_csv.h $(BUILDDIR)/csv/writer.h $(BUILDDIR)/expr/py_expr.h $(BUILDDIR)/py_column.h $(BUILDDIR)/py_columnset.h $(BUILDDIR)/py_datatable.h $(BUILDDIR)/py_datawindow.h $(BUILDDIR)/py_encodings.h $(BUILDDIR)/py_rowindex.h $(BUILDDIR)/py_types.h $(BUILDDIR)/py_utils.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/encodings.o : c/encodings.c $(BUILDDIR)/encodings.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/expr/binaryop.o : c/expr/binaryop.cc $(BUILDDIR)/expr/py_expr.h $(BUILDDIR)/types.h $(BUILDDIR)/utils/exceptions.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/expr/py_expr.o : c/expr/py_expr.cc $(BUILDDIR)/expr/py_expr.h $(BUILDDIR)/py_column.h $(BUILDDIR)/utils/pyobj.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/expr/reduceop.o : c/expr/reduceop.cc $(BUILDDIR)/expr/py_expr.h $(BUILDDIR)/types.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/expr/unaryop.o : c/expr/unaryop.cc $(BUILDDIR)/expr/py_expr.h $(BUILDDIR)/types.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/memorybuf.o : c/memorybuf.cc $(BUILDDIR)/datatable_check.h $(BUILDDIR)/memorybuf.h $(BUILDDIR)/py_utils.h $(BUILDDIR)/utils.h $(BUILDDIR)/utils/assert.h $(BUILDDIR)/utils/file.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/mmm.o : c/mmm.cc $(BUILDDIR)/mmm.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/py_buffers.o : c/py_buffers.cc $(BUILDDIR)/column.h $(BUILDDIR)/encodings.h $(BUILDDIR)/py_column.h $(BUILDDIR)/py_datatable.h $(BUILDDIR)/py_types.h $(BUILDDIR)/py_utils.h $(BUILDDIR)/utils/exceptions.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/py_column.o : c/py_column.cc $(BUILDDIR)/py_column.h $(BUILDDIR)/py_types.h $(BUILDDIR)/sort.h $(BUILDDIR)/utils/pyobj.h $(BUILDDIR)/writebuf.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/py_columnset.o : c/py_columnset.cc $(BUILDDIR)/columnset.h $(BUILDDIR)/py_column.h $(BUILDDIR)/py_columnset.h $(BUILDDIR)/py_datatable.h $(BUILDDIR)/py_rowindex.h $(BUILDDIR)/py_types.h $(BUILDDIR)/py_utils.h $(BUILDDIR)/utils.h $(BUILDDIR)/utils/pyobj.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/py_datatable.o : c/py_datatable.cc $(BUILDDIR)/datatable.h $(BUILDDIR)/datatable_check.h $(BUILDDIR)/py_column.h $(BUILDDIR)/py_columnset.h $(BUILDDIR)/py_datatable.h $(BUILDDIR)/py_datawindow.h $(BUILDDIR)/py_rowindex.h $(BUILDDIR)/py_types.h $(BUILDDIR)/py_utils.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/py_datatable_fromlist.o : c/py_datatable_fromlist.c $(BUILDDIR)/column.h $(BUILDDIR)/memorybuf.h $(BUILDDIR)/py_datatable.h $(BUILDDIR)/py_types.h $(BUILDDIR)/py_utils.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/py_datawindow.o : c/py_datawindow.c $(BUILDDIR)/datatable.h $(BUILDDIR)/py_datatable.h $(BUILDDIR)/py_datawindow.h $(BUILDDIR)/py_types.h $(BUILDDIR)/py_utils.h $(BUILDDIR)/rowindex.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/py_encodings.o : c/py_encodings.c $(BUILDDIR)/py_encodings.h $(BUILDDIR)/py_utils.h $(BUILDDIR)/utils/assert.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/py_rowindex.o : c/py_rowindex.cc $(BUILDDIR)/py_column.h $(BUILDDIR)/py_datatable.h $(BUILDDIR)/py_rowindex.h $(BUILDDIR)/py_utils.h $(BUILDDIR)/utils/pyobj.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/py_types.o : c/py_types.c $(BUILDDIR)/py_types.h $(BUILDDIR)/py_utils.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/py_utils.o : c/py_utils.c $(BUILDDIR)/py_datatable.h $(BUILDDIR)/py_utils.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/python/list.o : c/python/list.cc $(BUILDDIR)/python/list.h $(BUILDDIR)/utils/exceptions.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/rowindex.o : c/rowindex.cc $(BUILDDIR)/datatable_check.h $(BUILDDIR)/rowindex.h $(BUILDDIR)/utils.h $(BUILDDIR)/utils/assert.h $(BUILDDIR)/utils/omp.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/rowindex_array.o : c/rowindex_array.cc $(BUILDDIR)/column.h $(BUILDDIR)/datatable_check.h $(BUILDDIR)/memorybuf.h $(BUILDDIR)/rowindex.h $(BUILDDIR)/utils/assert.h $(BUILDDIR)/utils/exceptions.h $(BUILDDIR)/utils/omp.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/rowindex_slice.o : c/rowindex_slice.cc $(BUILDDIR)/datatable_check.h $(BUILDDIR)/rowindex.h $(BUILDDIR)/utils/exceptions.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/sort.o : c/sort.cc $(BUILDDIR)/column.h $(BUILDDIR)/rowindex.h $(BUILDDIR)/sort.h $(BUILDDIR)/types.h $(BUILDDIR)/utils.h $(BUILDDIR)/utils/assert.h $(BUILDDIR)/utils/omp.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/stats.o : c/stats.cc $(BUILDDIR)/column.h $(BUILDDIR)/rowindex.h $(BUILDDIR)/stats.h $(BUILDDIR)/utils.h $(BUILDDIR)/utils/omp.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/types.o : c/types.c $(BUILDDIR)/types.h $(BUILDDIR)/utils.h $(BUILDDIR)/utils/assert.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/utils.o : c/utils.c $(BUILDDIR)/utils.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/utils/exceptions.o : c/utils/exceptions.cc $(BUILDDIR)/utils/exceptions.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/utils/file.o : c/utils/file.cc $(BUILDDIR)/utils.h $(BUILDDIR)/utils/file.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/utils/pyobj.o : c/utils/pyobj.cc $(BUILDDIR)/py_column.h $(BUILDDIR)/py_datatable.h $(BUILDDIR)/py_rowindex.h $(BUILDDIR)/py_types.h $(BUILDDIR)/utils/exceptions.h $(BUILDDIR)/utils/pyobj.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/writebuf.o : c/writebuf.cc $(BUILDDIR)/memorybuf.h $(BUILDDIR)/utils.h $(BUILDDIR)/utils/omp.h $(BUILDDIR)/writebuf.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@
