CREATE DATABASE regtest_cheat_funcs_eucjp ENCODING 'EUC_JP' TEMPLATE template0;
\c regtest_cheat_funcs_eucjp

SET client_encoding TO 'EUC_JP';

CREATE EXTENSION pg_cheat_funcs;

\pset null '(null)'

SELECT pg_eucjp(65::bit(8));
SELECT pg_eucjp(65::bit(8), 65::bit(8));

SELECT pg_eucjp('xa4', 'xca');
SELECT pg_eucjp('xa4', 'xca', 'xca');
SELECT pg_eucjp('xa0', 'xca');
SELECT pg_eucjp('xa4', 'xff');

SELECT pg_eucjp('x8e', 'xc5');
SELECT pg_eucjp('x8e', 'xc5', 'xc5');
SELECT pg_eucjp('x8e', 'xe0');

SELECT pg_eucjp('x8f', 'xa2', 'xee');
SELECT pg_eucjp('x8f', 'xa0', 'xee');
SELECT pg_eucjp('x8f', 'xa2', 'xff');

CREATE TABLE all_eucjp(code1 text, code2 text, code3 text, eucjp text);
INSERT INTO all_eucjp SELECT * FROM pg_all_eucjp();
SELECT count(*) FROM all_eucjp
    WHERE pg_eucjp(code1::bit(8), code2::bit(8), code3::bit(8)) <> eucjp;
SELECT * FROM all_eucjp ORDER BY eucjp;

DROP EXTENSION pg_cheat_funcs;

\c contrib_regression
DROP DATABASE regtest_cheat_funcs_eucjp;
