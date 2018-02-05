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

capi_h: c/capi.h
column_h: c/column.h memorybuf_h rowindex_h stats_h types_h
columnset_h: c/columnset.h column_h datatable_h rowindex_h types_h
csv_dtoa_h: c/csv/dtoa.h
csv_fread_h: c/csv/fread.h csv_reader_h memorybuf_h utils_h utils_omp_h
csv_freadLookups_h: c/csv/freadLookups.h
csv_itoa_h: c/csv/itoa.h
csv_py_csv_h: c/csv/py_csv.h py_utils_h
csv_reader_arff_h: c/csv/reader_arff.h csv_reader_h
csv_reader_fread_h: c/csv/reader_fread.h csv_fread_h csv_py_csv_h csv_reader_h memorybuf_h
csv_reader_h: c/csv/reader.h column_h datatable_h memorybuf_h utils_pyobj_h writebuf_h
csv_reader_parsers_h: c/csv/reader_parsers.h csv_fread_h
csv_writer_h: c/csv/writer.h datatable_h utils_h writebuf_h
datatable_check_h: c/datatable_check.h
datatable_h: c/datatable.h column_h datatable_check_h types_h
encodings_h: c/encodings.h
expr_py_expr_h: c/expr/py_expr.h column_h py_utils_h
memorybuf_h: c/memorybuf.h datatable_check_h mmm_h writebuf_h
mmm_h: c/mmm.h
py_column_h: c/py_column.h column_h py_datatable_h py_utils_h
py_columnset_h: c/py_columnset.h datatable_h py_utils_h rowindex_h
py_datatable_h: c/py_datatable.h datatable_h py_utils_h
py_datawindow_h: c/py_datawindow.h
py_encodings_h: c/py_encodings.h encodings_h
py_rowindex_h: c/py_rowindex.h rowindex_h
py_types_h: c/py_types.h datatable_h types_h utils_assert_h
py_utils_h: c/py_utils.h utils_h utils_exceptions_h
rowindex_h: c/rowindex.h column_h datatable_h
sort_h: c/sort.h
stats_h: c/stats.h datatable_check_h types_h
types_h: c/types.h
utils_assert_h: c/utils/assert.h
utils_exceptions_h: c/utils/exceptions.h types_h
utils_file_h: c/utils/file.h
utils_h: c/utils.h utils_exceptions_h
utils_omp_h: c/utils/omp.h
utils_pyobj_h: c/utils/pyobj.h
writebuf_h: c/writebuf.h utils_file_h


$(BUILDDIR)/capi.o : c/capi.cc capi_h datatable_h rowindex_h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/column.o : c/column.cc column_h datatable_check_h py_utils_h rowindex_h sort_h utils_h utils_assert_h utils_file_h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/column_bool.o : c/column_bool.cc column_h utils_omp_h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/column_from_pylist.o : c/column_from_pylist.cc column_h py_types_h utils_h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/column_fw.o : c/column_fw.cc column_h utils_h utils_assert_h utils_omp_h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/column_int.o : c/column_int.cc column_h py_types_h py_utils_h utils_omp_h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/column_pyobj.o : c/column_pyobj.cc column_h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/column_real.o : c/column_real.cc column_h py_utils_h utils_omp_h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/column_string.o : c/column_string.cc column_h datatable_check_h encodings_h py_utils_h utils_h utils_assert_h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/columnset.o : c/columnset.cc columnset_h utils_h utils_exceptions_h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/csv/fread.o : c/csv/fread.cc csv_fread_h csv_freadLookups_h csv_reader_h csv_reader_fread_h csv_reader_parsers_h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/csv/py_csv.o : c/csv/py_csv.cc csv_py_csv_h csv_reader_h csv_writer_h py_datatable_h py_utils_h utils_h utils_omp_h utils_pyobj_h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/csv/reader.o : c/csv/reader.cc csv_reader_h csv_reader_arff_h csv_reader_fread_h utils_exceptions_h utils_omp_h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/csv/reader_arff.o : c/csv/reader_arff.cc csv_reader_arff_h utils_exceptions_h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/csv/reader_fread.o : c/csv/reader_fread.cc column_h csv_fread_h csv_reader_h csv_reader_fread_h csv_reader_parsers_h datatable_h py_datatable_h py_encodings_h py_utils_h utils_h utils_assert_h utils_file_h utils_pyobj_h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/csv/reader_parsers.o : c/csv/reader_parsers.cc csv_reader_parsers_h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/csv/reader_utils.o : c/csv/reader_utils.cc csv_fread_h csv_reader_h utils_exceptions_h utils_omp_h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/csv/writer.o : c/csv/writer.cc column_h csv_dtoa_h csv_itoa_h csv_writer_h datatable_h memorybuf_h types_h utils_h utils_omp_h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/datatable.o : c/datatable.cc datatable_h datatable_check_h py_utils_h rowindex_h types_h utils_omp_h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/datatable_cbind.o : c/datatable_cbind.cc datatable_h rowindex_h utils_h utils_assert_h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/datatable_check.o : c/datatable_check.cc datatable_check_h utils_h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/datatable_load.o : c/datatable_load.cc datatable_h utils_h utils_assert_h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/datatable_rbind.o : c/datatable_rbind.cc column_h datatable_h utils_h utils_assert_h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/datatablemodule.o : c/datatablemodule.c capi_h csv_py_csv_h csv_writer_h expr_py_expr_h py_column_h py_columnset_h py_datatable_h py_datawindow_h py_encodings_h py_rowindex_h py_types_h py_utils_h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/encodings.o : c/encodings.c encodings_h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/expr/binaryop.o : c/expr/binaryop.cc expr_py_expr_h types_h utils_exceptions_h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/expr/py_expr.o : c/expr/py_expr.cc expr_py_expr_h py_column_h utils_pyobj_h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/expr/reduceop.o : c/expr/reduceop.cc expr_py_expr_h types_h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/expr/unaryop.o : c/expr/unaryop.cc expr_py_expr_h types_h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/memorybuf.o : c/memorybuf.cc datatable_check_h memorybuf_h py_utils_h utils_h utils_assert_h utils_file_h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/mmm.o : c/mmm.cc mmm_h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/py_buffers.o : c/py_buffers.cc column_h encodings_h py_column_h py_datatable_h py_types_h py_utils_h utils_exceptions_h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/py_column.o : c/py_column.cc py_column_h py_types_h sort_h utils_pyobj_h writebuf_h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/py_columnset.o : c/py_columnset.cc columnset_h py_column_h py_columnset_h py_datatable_h py_rowindex_h py_types_h py_utils_h utils_h utils_pyobj_h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/py_datatable.o : c/py_datatable.cc datatable_h datatable_check_h py_column_h py_columnset_h py_datatable_h py_datawindow_h py_rowindex_h py_types_h py_utils_h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/py_datatable_fromlist.o : c/py_datatable_fromlist.c column_h memorybuf_h py_datatable_h py_types_h py_utils_h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/py_datawindow.o : c/py_datawindow.c datatable_h py_datatable_h py_datawindow_h py_types_h py_utils_h rowindex_h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/py_encodings.o : c/py_encodings.c py_encodings_h py_utils_h utils_assert_h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/py_rowindex.o : c/py_rowindex.c py_column_h py_datatable_h py_rowindex_h py_utils_h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/py_types.o : c/py_types.c py_types_h py_utils_h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/py_utils.o : c/py_utils.c py_datatable_h py_utils_h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/rowindex.o : c/rowindex.cc datatable_check_h rowindex_h types_h utils_h utils_assert_h utils_omp_h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/sort.o : c/sort.cc column_h rowindex_h sort_h types_h utils_h utils_assert_h utils_omp_h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/stats.o : c/stats.cc column_h rowindex_h stats_h utils_h utils_omp_h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/types.o : c/types.c types_h utils_h utils_assert_h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/utils.o : c/utils.c utils_h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/utils/exceptions.o : c/utils/exceptions.cc utils_exceptions_h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/utils/file.o : c/utils/file.cc utils_h utils_file_h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/utils/pyobj.o : c/utils/pyobj.cc py_column_h py_datatable_h py_types_h utils_exceptions_h utils_pyobj_h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@

$(BUILDDIR)/writebuf.o : c/writebuf.cc memorybuf_h utils_h utils_omp_h writebuf_h
	@echo • Compiling $<
	@$(CC) -c $< $(CCFLAGS) -o $@
