\echo '============================================================'
\echo 'LSM3 MULTI-LEVEL STRUCTURAL TEST'
\echo '============================================================'

DROP TABLE IF EXISTS lsm3_level_test CASCADE;

CREATE TABLE lsm3_level_test
(
    id integer,
    payload text
);

\echo ''
\echo 'Step 1: create LSM3 index with num_levels = 3'
\echo 'Expected on updated implementation: success'
\echo 'Expected on original implementation: ERROR, because num_levels is unknown'
\echo ''

CREATE INDEX lsm3_level_test_idx
ON lsm3_level_test
USING lsm3(id)
WITH (
    num_levels = 3,
    level_size_ratio = 4,
    top_index_size = 8
);

\echo ''
\echo 'Step 2: list physical indexes created for this logical LSM index'
\echo 'Expected updated result:'
\echo '  lsm3_level_test_idx'
\echo '  lsm3_level_test_idx_top0'
\echo '  lsm3_level_test_idx_top1'
\echo '  lsm3_level_test_idx_level0'
\echo '  lsm3_level_test_idx_level1'
\echo '  lsm3_level_test_idx_level2'
\echo ''

SELECT
    c.relname,
    pg_size_pretty(pg_relation_size(c.oid)) AS size
FROM pg_class c
JOIN pg_namespace n ON n.oid = c.relnamespace
WHERE c.relname LIKE 'lsm3_level_test_idx%'
ORDER BY c.relname;

\echo ''
\echo 'Step 3: insert rows'
\echo 'We insert more than 64K rows because LSM3 checks top-index overflow only every 64K inserts.'
\echo ''

INSERT INTO lsm3_level_test
SELECT
    g,
    md5(g::text)
FROM generate_series(1, 80000) AS g;

ANALYZE lsm3_level_test;

\echo ''
\echo 'Step 4: verify correctness of indexed point queries'
\echo 'Expected: each query returns exactly one row'
\echo ''

SET enable_seqscan = off;

EXPLAIN SELECT * FROM lsm3_level_test WHERE id = 10;
SELECT * FROM lsm3_level_test WHERE id = 10;

EXPLAIN SELECT * FROM lsm3_level_test WHERE id = 40000;
SELECT * FROM lsm3_level_test WHERE id = 40000;

EXPLAIN SELECT * FROM lsm3_level_test WHERE id = 79999;
SELECT * FROM lsm3_level_test WHERE id = 79999;

\echo ''
\echo 'Step 5: verify range query correctness'
\echo 'Expected count: 101'
\echo ''

SELECT count(*) AS expected_101
FROM lsm3_level_test
WHERE id BETWEEN 10000 AND 10100;

\echo ''
\echo 'Step 6: inspect physical component sizes after insert'
\echo 'For this first patch, level indexes are expected to exist but remain nearly empty.'
\echo 'Top indexes and/or base index may grow depending on whether merge has happened.'
\echo ''

SELECT
    c.relname,
    pg_size_pretty(pg_relation_size(c.oid)) AS size,
    pg_relation_size(c.oid) AS bytes
FROM pg_class c
JOIN pg_namespace n ON n.oid = c.relnamespace
WHERE c.relname LIKE 'lsm3_level_test_idx%'
ORDER BY c.relname;

\echo ''
\echo 'Step 7: optional merge count'
\echo 'Expected: may be 0 or more depending on whether the small top_index_size triggered merge.'
\echo ''

SELECT lsm3_get_merge_count('lsm3_level_test_idx'::regclass::oid) AS merge_count;

\echo ''
\echo 'Step 8: reconnect/restart test instruction'
\echo 'Now disconnect and reconnect, then run the catalog query again.'
\echo 'The updated code should rediscover top and level indexes by name.'
\echo ''

RESET enable_seqscan;