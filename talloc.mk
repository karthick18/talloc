TALLOC_OBJ = $(tallocdir)/talloc.o 

TALLOC_SOLIB = libtalloc.$(SHLIBEXT).$(TALLOC_VERSION)
TALLOC_SONAME = libtalloc.$(SHLIBEXT).$(TALLOC_VERSION_MAJOR)
TALLOC_STLIB = libtalloc.a

all:: $(TALLOC_STLIB) $(TALLOC_SOLIB) testsuite talloc_test

testsuite:: $(LIBOBJ) testsuite.o testsuite_main.o
	$(CC) $(CFLAGS) -o testsuite testsuite.o testsuite_main.o $(LIBOBJ) $(LIBS)

talloc_test:: $(LIBOBJ) talloc_test.o
	$(CC) $(CFLAGS) -o talloc_test talloc_test.o $(LIBOBJ) $(LIBS)

$(TALLOC_STLIB): $(LIBOBJ)
	ar -rv $@ $(LIBOBJ)
	@-ranlib $@

install:: all 
	${INSTALLCMD} -d $(DESTDIR)$(libdir)
	${INSTALLCMD} -d $(DESTDIR)$(libdir)/pkgconfig
	${INSTALLCMD} -m 755 $(TALLOC_STLIB) $(DESTDIR)$(libdir)
	${INSTALLCMD} -m 755 $(TALLOC_SOLIB) $(DESTDIR)$(libdir)
	${INSTALLCMD} -d $(DESTDIR)${includedir}
	${INSTALLCMD} -m 644 $(srcdir)/talloc.h $(DESTDIR)$(includedir)
	${INSTALLCMD} -m 644 talloc.pc $(DESTDIR)$(libdir)/pkgconfig
	if [ -f talloc.3 ];then ${INSTALLCMD} -d $(DESTDIR)$(mandir)/man3; fi
	if [ -f talloc.3 ];then ${INSTALLCMD} -m 644 talloc.3 $(DESTDIR)$(mandir)/man3; fi
	which swig >/dev/null 2>&1 && ${INSTALLCMD} -d $(DESTDIR)`swig -swiglib` || true
	which swig >/dev/null 2>&1 && ${INSTALLCMD} -m 644 talloc.i $(DESTDIR)`swig -swiglib` || true

doc:: talloc.3 talloc.3.html

clean::
	rm -f *~ $(LIBOBJ) $(TALLOC_SOLIB) $(TALLOC_STLIB) talloc_test talloc_test.o testsuite testsuite.o testsuite_main.o *.gc?? talloc.3 talloc.3.html
	rm -fr abi
	rm -f talloc.exports.sort talloc.exports.check talloc.exports.check.sort
	rm -f talloc.signatures.sort talloc.signatures.check talloc.signatures.check.sort

test:: testsuite talloc_test
	./testsuite
	./talloc_test

abi_checks::
	@echo ABI checks:
	@./script/abi_checks.sh talloc talloc.h

test:: abi_checks

gcov::
	gcov talloc.c
