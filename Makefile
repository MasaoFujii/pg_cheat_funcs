MODULE_big	= pg_cheat_funcs
OBJS = pg_cheat_funcs.o

EXTENSION = pg_cheat_funcs
DATA = pg_cheat_funcs--1.0.sql

REGRESS = pg_cheat_funcs

MAJORVERSION_INT = $(shell echo $(MAJORVERSION) | tr -d .)
REGRESS += $(shell if [ $(MAJORVERSION_INT) -ge 95 ]; then echo pglz_compress; fi)
REGRESS += $(shell if [ $(MAJORVERSION_INT) -ge 94 ]; then echo pg_chr; else echo pg_chr_91_93; fi)

PGFILEDESC = "pg_cheat_funcs - provides cheat (but useful) functions"

ifdef USE_PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir = contrib/pg_cheat_funcs
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif
