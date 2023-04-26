MODULE_big	= pg_cheat_funcs
OBJS = pg_cheat_funcs.o

EXTENSION = pg_cheat_funcs
DATA = pg_cheat_funcs--1.0.sql

REGRESS = pg_cheat_funcs pg_eucjp

MAJORVERSION_INT = $(shell echo $(MAJORVERSION).0 | cut -d . -f 1-2 | tr -d .)
REGRESS += $(shell if [ $(MAJORVERSION_INT) -ge 100 ]; then echo pg_saslprep; fi)
REGRESS += $(shell if [ $(MAJORVERSION_INT) -ge 96 -a $(MAJORVERSION_INT) -lt 140 ]; then echo pg_stat_get_memory_context; fi)
REGRESS += $(shell if [ $(MAJORVERSION_INT) -lt 140 ]; then echo pg_stat_print_memory_context; fi)
REGRESS += $(shell if [ $(MAJORVERSION_INT) -ge 95 ]; then echo pglz_compress; fi)
REGRESS += $(shell if [ $(MAJORVERSION_INT) -ge 94 ]; then echo pg_chr pg_94_or_later; else echo pg_chr_91_93; fi)
REGRESS += $(shell if [ $(MAJORVERSION_INT) -ge 92 ]; then echo pg_cached_plan; fi)
REGRESS += $(shell if [ $(MAJORVERSION_INT) -ge 130 ]; then echo pg_xid_to_xid8; fi)
REGRESS += $(shell if [ $(MAJORVERSION_INT) -lt 160 ]; then echo pg_advance_vacuum_cleanup_age; fi)

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
