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


clean:
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
	rowindex.o                \
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

$(BUILDDIR)/_datatable.so: $(fast_objects)
	@echo • Linking object files into _datatable.so
	@$(CC) $(LDFLAGS) -o $@ $+
	@$(MAKE) --no-print-directory post-fast

$(BUILDDIR)/capi.o : c/capi.cc c/rowindex.h c/capi.h c/datatable.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/column.o : c/column.cc c/rowindex.h c/py_utils.h c/utils/file.h c/datatable_check.h c/column.h c/utils.h c/utils/assert.h c/sort.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/column_bool.o : c/column_bool.cc c/column.h c/utils/omp.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/column_from_pylist.o : c/column_from_pylist.cc c/py_types.h c/column.h c/utils.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/column_fw.o : c/column_fw.cc c/column.h c/utils.h c/utils/omp.h c/utils/assert.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/column_int.o : c/column_int.cc c/column.h c/py_types.h c/py_utils.h c/utils/omp.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/column_pyobj.o : c/column_pyobj.cc c/column.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/column_real.o : c/column_real.cc c/column.h c/py_utils.h c/utils/omp.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/column_string.o : c/column_string.cc c/column.h c/datatable_check.h c/encodings.h c/py_utils.h c/utils.h c/utils/assert.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/columnset.o : c/columnset.cc c/columnset.h c/utils.h c/utils/exceptions.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/csv/fread.o : c/csv/fread.cc c/csv/fread.h c/csv/freadLookups.h c/csv/reader.h c/csv/reader_fread.h c/csv/reader_parsers.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/csv/py_csv.o : c/csv/py_csv.cc c/csv/py_csv.h c/csv/reader.h c/csv/writer.h c/py_datatable.h c/py_utils.h c/utils.h c/utils/omp.h c/utils/pyobj.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/csv/reader.o : c/csv/reader.cc c/csv/reader.h c/csv/reader_arff.h c/csv/reader_fread.h c/utils/exceptions.h c/utils/omp.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/csv/reader_arff.o : c/csv/reader_arff.cc c/csv/reader_arff.h c/utils/exceptions.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/csv/reader_fread.o : c/csv/reader_fread.cc c/column.h c/csv/fread.h c/csv/reader.h c/csv/reader_fread.h c/csv/reader_parsers.h c/datatable.h c/py_datatable.h c/py_encodings.h c/py_utils.h c/utils.h c/utils/assert.h c/utils/file.h c/utils/pyobj.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/csv/reader_parsers.o : c/csv/reader_parsers.cc c/csv/reader_parsers.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/csv/reader_utils.o : c/csv/reader_utils.cc c/csv/fread.h c/csv/reader.h c/utils/exceptions.h c/utils/omp.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/csv/writer.o : c/csv/writer.cc c/column.h c/csv/dtoa.h c/csv/itoa.h c/csv/writer.h c/datatable.h c/memorybuf.h c/types.h c/utils.h c/utils/omp.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/datatable.o : c/datatable.cc c/datatable.h c/datatable_check.h c/py_utils.h c/rowindex.h c/types.h c/utils/omp.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/datatable_cbind.o : c/datatable_cbind.cc c/datatable.h c/rowindex.h c/utils.h c/utils/assert.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/datatable_check.o : c/datatable_check.cc c/datatable_check.h c/utils.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/datatable_load.o : c/datatable_load.cc c/datatable.h c/utils.h c/utils/assert.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/datatable_rbind.o : c/datatable_rbind.cc c/column.h c/datatable.h c/utils.h c/utils/assert.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/datatablemodule.o : c/datatablemodule.c c/capi.h c/csv/py_csv.h c/csv/writer.h c/expr/py_expr.h c/py_column.h c/py_columnset.h c/py_datatable.h c/py_datawindow.h c/py_encodings.h c/py_rowindex.h c/py_types.h c/py_utils.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/encodings.o : c/encodings.c c/encodings.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/expr/binaryop.o : c/expr/binaryop.cc c/expr/py_expr.h c/types.h c/utils/exceptions.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/expr/py_expr.o : c/expr/py_expr.cc c/expr/py_expr.h c/py_column.h c/utils/pyobj.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/expr/reduceop.o : c/expr/reduceop.cc c/expr/py_expr.h c/types.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/expr/unaryop.o : c/expr/unaryop.cc c/expr/py_expr.h c/types.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/memorybuf.o : c/memorybuf.cc c/datatable_check.h c/memorybuf.h c/py_utils.h c/utils.h c/utils/assert.h c/utils/file.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/mmm.o : c/mmm.cc c/mmm.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/py_buffers.o : c/py_buffers.cc c/column.h c/encodings.h c/py_column.h c/py_datatable.h c/py_types.h c/py_utils.h c/utils/exceptions.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/py_column.o : c/py_column.cc c/py_column.h c/sort.h c/py_types.h c/writebuf.h c/utils/pyobj.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/py_columnset.o : c/py_columnset.cc c/columnset.h c/py_column.h c/py_columnset.h c/py_datatable.h c/py_rowindex.h c/py_types.h c/py_utils.h c/utils.h c/utils/pyobj.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/py_datatable.o : c/py_datatable.cc c/datatable.h c/datatable_check.h c/py_column.h c/py_columnset.h c/py_datatable.h c/py_datawindow.h c/py_rowindex.h c/py_types.h c/py_utils.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/py_datatable_fromlist.o : c/py_datatable_fromlist.c c/column.h c/memorybuf.h c/py_datatable.h c/py_types.h c/py_utils.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/py_datawindow.o : c/py_datawindow.c c/datatable.h c/py_datatable.h c/py_datawindow.h c/py_types.h c/py_utils.h c/rowindex.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/py_encodings.o : c/py_encodings.c c/py_encodings.h c/py_utils.h c/utils/assert.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/py_rowindex.o : c/py_rowindex.c c/py_column.h c/py_datatable.h c/py_rowindex.h c/py_utils.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/py_types.o : c/py_types.c c/py_types.h c/py_utils.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/py_utils.o : c/py_utils.c c/py_datatable.h c/py_utils.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/rowindex.o : c/rowindex.cc c/column.h c/datatable.h c/datatable_check.h c/rowindex.h c/types.h c/utils.h c/utils/assert.h c/utils/omp.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/sort.o : c/sort.cc c/column.h c/rowindex.h c/sort.h c/types.h c/utils.h c/utils/assert.h c/utils/omp.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/stats.o : c/stats.cc c/column.h c/rowindex.h c/stats.h c/utils.h c/utils/omp.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/types.o : c/types.c c/types.h c/utils.h c/utils/assert.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/utils.o : c/utils.c c/utils.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/utils/exceptions.o : c/utils/exceptions.cc c/utils/exceptions.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/utils/file.o : c/utils/file.cc c/utils.h c/utils/file.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/utils/pyobj.o : c/utils/pyobj.cc c/py_column.h c/py_datatable.h c/py_types.h c/utils/exceptions.h c/utils/pyobj.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/writebuf.o : c/writebuf.cc c/memorybuf.h c/utils.h c/utils/omp.h c/writebuf.h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

