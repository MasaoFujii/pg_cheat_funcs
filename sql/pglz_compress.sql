CREATE EXTENSION pg_cheat_funcs;

\pset null '(null)'

SELECT lz, aa, pglz_decompress(lz), aa::bytea, pglz_decompress_bytea(lz)
FROM (SELECT aa, pglz_compress(aa) lz FROM repeat('A', 2) aa) hoge;
SELECT lz, aa, pglz_decompress(lz), aa::bytea, pglz_decompress_bytea(lz)
FROM (SELECT aa, pglz_compress(aa) lz FROM repeat('A', 32) aa) hoge;

SELECT lz, aa, pglz_decompress_bytea(lz), pglz_decompress(lz)
FROM (SELECT aa::bytea, pglz_compress_bytea(aa::bytea) lz FROM repeat('A', 2) aa) hoge;
SELECT lz, aa, pglz_decompress_bytea(lz), pglz_decompress(lz)
FROM (SELECT aa::bytea, pglz_compress_bytea(aa::bytea) lz FROM repeat('A', 32) aa) hoge;

DROP EXTENSION pg_cheat_funcs;
