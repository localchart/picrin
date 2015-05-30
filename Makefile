BENZ_SRCS = $(wildcard extlib/benz/*.c)
BENZ_OBJS = $(BENZ_SRCS:.c=.o)

PICRIN_SRCS = \
	src/main.c\
	src/load_piclib.c\
	src/init_contrib.c
PICRIN_OBJS = \
	$(PICRIN_SRCS:.c=.o)
PICRIN_LIBS = \
	piclib/picrin/base.scm\
	piclib/picrin/macro.scm\
	piclib/picrin/record.scm\
	piclib/picrin/array.scm\
	piclib/picrin/dictionary.scm\
	piclib/picrin/experimental/lambda.scm\
	piclib/picrin/syntax-rules.scm\
	piclib/picrin/test.scm

CONTRIB_SRCS =
CONTRIB_OBJS = $(CONTRIB_SRCS:.c=.o)
CONTRIB_LIBS =
CONTRIB_INITS =
CONTRIB_TESTS =
CONTRIB_DOCS = $(wildcard contrib/*/docs/*.rst)

CFLAGS += -I./extlib/benz/include
# CFLAGS += -std=c89 -ansi -pedantic
LDFLAGS += -lm

prefix = /usr/local

all: CFLAGS += -O2 -Wall -Wextra
all: bin/picrin

include contrib/*/nitro.mk

debug: CFLAGS += -O0 -g -DDEBUG=1
debug: bin/picrin

bin/picrin: $(PICRIN_OBJS) $(CONTRIB_OBJS) lib/libbenz.a
	$(CC) $(CFLAGS) -o $@ $(PICRIN_OBJS) $(CONTRIB_OBJS) lib/libbenz.a $(LDFLAGS)

src/load_piclib.c: $(PICRIN_LIBS) $(CONTRIB_LIBS)
	perl etc/mkloader.pl $(PICRIN_LIBS) $(CONTRIB_LIBS) > $@

src/init_contrib.c:
	perl etc/mkinit.pl $(CONTRIB_INITS) > $@

lib/libbenz.a: $(BENZ_OBJS)
	$(AR) $(ARFLAGS) $@ $(BENZ_OBJS)

%.o: extlib/benz/include/picrin.h extlib/benz/include/picrin/*.h

doc: docs/*.rst docs/contrib.rst
	$(MAKE) -C docs html
	mkdir -p doc
	cp -uR docs/_build/* -t doc/

docs/contrib.rst: $(CONTRIB_DOCS)
	echo "Contrib Libraries \\\(a.k.a nitros\\\)" > $@
	echo "================================" >> $@
	echo "" >> $@
	cat $(CONTRIB_DOCS) >> $@

run: bin/picrin
	bin/picrin

test: test-r7rs test-contribs

test-r7rs: bin/picrin t/r7rs-tests.scm
	bin/picrin t/r7rs-tests.scm

test-contribs: bin/picrin $(CONTRIB_TESTS)

install: all
	install -c bin/picrin $(prefix)/bin/picrin

clean:
	rm -f src/load_piclib.c src/init_contrib.c
	rm -f lib/libbenz.a
	rm -f $(BENZ_OBJS)
	rm -f $(PICRIN_OBJS)
	rm -f $(CONTRIB_OBJS)

.PHONY: all insall clean run test test-r7rs test-contribs doc $(CONTRIB_TESTS)
