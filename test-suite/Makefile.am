## Process this file with automake to produce Makefile.in.
##
## Copyright 2001-2020, 2023, 2024 Software Foundation, Inc.
##
## This file is part of GUILE.
##
##   GUILE is free software; you can redistribute it and/or modify it
##   under the terms of the GNU Lesser General Public License as
##   published by the Free Software Foundation; either version 3, or
##   (at your option) any later version.
##
##   GUILE is distributed in the hope that it will be useful, but
##   WITHOUT ANY WARRANTY; without even the implied warranty of
##   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
##   GNU Lesser General Public License for more details.
##
##   You should have received a copy of the GNU Lesser General Public
##   License along with GUILE; see the file COPYING.LESSER.  If not,
##   write to the Free Software Foundation, Inc., 51 Franklin Street,
##   Fifth Floor, Boston, MA 02110-1301 USA

SUBDIRS = standalone

SCM_TESTS = tests/00-initial-env.test		\
	    tests/00-repl-server.test		\
	    tests/00-socket.test		\
            tests/alist.test			\
	    tests/and-let-star.test		\
	    tests/arrays.test			\
	    tests/bit-operations.test		\
	    tests/bitvectors.test		\
	    tests/brainfuck.test		\
	    tests/bytevectors.test		\
	    tests/c-api.test			\
	    tests/chars.test			\
	    tests/coding.test			\
	    tests/common-list.test		\
	    tests/compiler.test			\
	    tests/control.test			\
	    tests/continuations.test		\
	    tests/copy-tree.test		\
	    tests/coverage.test			\
	    tests/cross-compilation.test	\
	    tests/curried-definitions.test	\
	    tests/dwarf.test			\
	    tests/ecmascript.test		\
	    tests/elisp.test			\
	    tests/elisp-compiler.test		\
	    tests/elisp-reader.test		\
	    tests/encoding-escapes.test		\
	    tests/encoding-iso88591.test	\
	    tests/encoding-iso88597.test	\
	    tests/encoding-utf8.test		\
	    tests/error-handling.test		\
	    tests/eval.test			\
	    tests/eval-string.test		\
	    tests/exceptions.test		\
	    tests/fdes-finalizers.test		\
	    tests/filesys.test			\
	    tests/fluids.test			\
	    tests/foreign.test			\
	    tests/format.test			\
	    tests/fractions.test		\
	    tests/ftw.test			\
	    tests/future.test			\
	    tests/gc.test			\
	    tests/getopt-long.test		\
	    tests/goops.test			\
	    tests/guardians.test		\
	    tests/hash.test			\
	    tests/hooks.test			\
	    tests/i18n.test			\
	    tests/iconv.test			\
	    tests/import.test			\
	    tests/interp.test			\
	    tests/keywords.test			\
	    tests/list.test			\
	    tests/load.test			\
	    tests/match.test			\
	    tests/modules.test			\
	    tests/net-db.test			\
	    tests/numbers.test			\
	    tests/optargs.test			\
	    tests/options.test			\
	    tests/pairs.test			\
	    tests/parameters.test		\
	    tests/peg.test			\
	    tests/peval.test			\
	    tests/print.test			\
	    tests/procprop.test			\
	    tests/procs.test			\
	    tests/poe.test			\
	    tests/popen.test			\
	    tests/ports.test			\
	    tests/posix.test			\
	    tests/q.test			\
	    tests/r4rs.test			\
	    tests/r5rs_pitfall.test		\
	    tests/r6rs-arithmetic-bitwise.test	\
	    tests/r6rs-arithmetic-fixnums.test	\
	    tests/r6rs-arithmetic-flonums.test	\
	    tests/r6rs-base.test		\
	    tests/r6rs-conditions.test		\
	    tests/r6rs-control.test		\
	    tests/r6rs-enums.test		\
	    tests/r6rs-eval.test		\
	    tests/r6rs-exceptions.test		\
	    tests/r6rs-files.test		\
	    tests/r6rs-hashtables.test		\
	    tests/r6rs-lists.test		\
	    tests/r6rs-ports.test		\
	    tests/r6rs-records-inspection.test	\
	    tests/r6rs-records-procedural.test	\
	    tests/r6rs-records-syntactic.test	\
	    tests/r6rs-unicode.test		\
	    tests/rnrs-libraries.test		\
	    tests/array-map.test		\
	    tests/random.test			\
	    tests/rdelim.test			\
	    tests/reader.test			\
	    tests/records.test			\
	    tests/receive.test			\
	    tests/regexp.test			\
	    tests/rtl.test			\
	    tests/rtl-compilation.test		\
	    tests/sandbox.test			\
	    tests/session.test			\
	    tests/signals.test			\
	    tests/sort.test			\
	    tests/srcprop.test			\
	    tests/srfi-1.test			\
	    tests/srfi-2.test			\
	    tests/srfi-6.test			\
	    tests/srfi-10.test			\
	    tests/srfi-11.test			\
	    tests/srfi-13.test			\
	    tests/srfi-14.test			\
	    tests/srfi-17.test			\
	    tests/srfi-18.test			\
	    tests/srfi-19.test			\
	    tests/srfi-26.test			\
	    tests/srfi-27.test			\
	    tests/srfi-31.test			\
	    tests/srfi-34.test			\
	    tests/srfi-35.test			\
	    tests/srfi-37.test			\
	    tests/srfi-38.test			\
	    tests/srfi-39.test			\
	    tests/srfi-41.test			\
	    tests/srfi-42.test			\
	    tests/srfi-43.test			\
	    tests/srfi-45.test			\
	    tests/srfi-60.test			\
	    tests/srfi-64.test			\
	    tests/srfi-67.test			\
	    tests/srfi-69.test			\
	    tests/srfi-71.test			\
	    tests/srfi-88.test			\
	    tests/srfi-98.test			\
	    tests/srfi-105.test			\
	    tests/srfi-111.test			\
	    tests/srfi-119.test			\
	    tests/srfi-171.test			\
	    tests/srfi-4.test			\
	    tests/srfi-9.test			\
	    tests/statprof.test			\
	    tests/streams.test			\
	    tests/strings.test			\
	    tests/structs.test			\
	    tests/suspendable-ports.test	\
	    tests/sxml.fold.test		\
	    tests/sxml.match.test		\
	    tests/sxml.simple.test		\
	    tests/sxml.ssax.test		\
	    tests/sxml.transform.test		\
	    tests/sxml.xpath.test		\
	    tests/symbols.test			\
	    tests/syncase.test			\
	    tests/syntax.test			\
	    tests/texinfo.test			\
	    tests/texinfo.docbook.test		\
	    tests/texinfo.html.test		\
	    tests/texinfo.plain-text.test	\
	    tests/texinfo.serialize.test	\
	    tests/texinfo.string-utils.test	\
	    tests/threads.test			\
	    tests/time.test			\
	    tests/tree-il.test			\
	    tests/types.test			\
	    tests/unicode.test			\
	    tests/version.test			\
	    tests/vectors.test			\
	    tests/vlist.test			\
	    tests/vm.test			\
	    tests/weaks.test			\
	    tests/web-client.test		\
	    tests/web-http.test			\
	    tests/web-request.test		\
	    tests/web-response.test		\
	    tests/web-server.test		\
	    tests/web-uri.test

EXTRA_DIST = \
	guile-test \
	test-suite/lib.scm \
	test-suite/lib/automake.scm \
	$(SCM_TESTS) \
	tests/match.test.upstream \
	tests/multilingual.nottest \
	tests/popen-child.scm \
	tests/rnrs-test-a.scm \
	tests/srfi-64-test.scm \
	ChangeLog-2008


# Test suite of Dominique Boucher's `lalr-scm'.
# From http://code.google.com/p/lalr-scm/.

LALR_TESTS =					\
  lalr/test-glr-associativity.scm		\
  lalr/test-glr-basics-01.scm			\
  lalr/test-glr-basics-02.scm			\
  lalr/test-glr-basics-03.scm			\
  lalr/test-glr-basics-04.scm			\
  lalr/test-glr-basics-05.scm			\
  lalr/test-glr-script-expression.scm		\
  lalr/test-glr-single-expressions.scm		\
						\
  lalr/test-lr-associativity-01.scm		\
  lalr/test-lr-basics-01.scm			\
  lalr/test-lr-basics-02.scm			\
  lalr/test-lr-basics-03.scm			\
  lalr/test-lr-basics-04.scm			\
  lalr/test-lr-basics-05.scm			\
  lalr/test-lr-error-recovery-01.scm		\
  lalr/test-lr-error-recovery-02.scm		\
  lalr/test-lr-no-clause.scm			\
  lalr/test-lr-script-expression.scm		\
  lalr/test-lr-single-expressions.scm

# Tests not listed in `run-guile-test.sh' and which should not be run.
LALR_EXTRA =					\
  lalr/test-lr-associativity-02.scm		\
  lalr/test-lr-associativity-03.scm		\
  lalr/test-lr-associativity-04.scm

# Test framework.
LALR_EXTRA +=					\
  lalr/common-test.scm				\
  lalr/glr-test.scm				\
  lalr/run-guile-test.sh

TESTS = $(LALR_TESTS) $(SCM_TESTS)
TEST_EXTENSIONS = .scm .test
AM_TESTS_ENVIRONMENT = @LOCALCHARSET_TESTS_ENVIRONMENT@

# Run *.scm tests with meta/guile
SCM_LOG_COMPILER = $(top_builddir)/meta/guile
AM_SCM_LOG_FLAGS = --no-auto-compile

# Use a custom driver for *.test (assume they use (test-suite lib)).
# See the automake info pages regarding "Custom Test Drivers".
TEST_LOG_DRIVER = $(srcdir)/driver

EXTRA_DIST += $(LALR_EXTRA) $(LALR_TESTS) tests/sxml-match-tests.ss driver
