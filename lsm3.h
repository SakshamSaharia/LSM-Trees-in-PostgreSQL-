/*
 * It is too expensive to check index size at each insert because it requires traverse of all index file segments and calling lseek for each.
 * But we do not need precise size, so it is enough to do it at each n-th insert. The lagest B-Tree key size is abut 2kb,
 * so with N=64K in the worst case error will be less than 128Mb and for 32-bit key just 1Mb.
 */
#define LSM3_CHECK_TOP_INDEX_SIZE_PERIOD (64*1024) /* should be power of two */

/*
 * LSM3 MULTI-LEVEL CHANGE:
 * Keep shared-memory arrays fixed-size, but allow each Lsm3 index to use only entry->num_levels levels at runtime.
 * This avoids dynamic pointers in shared memory while supporting configurable LSM depth.
 */
#define LSM3_NUM_TOP_INDEXES 2
#define LSM3_MAX_LEVELS 8
#define LSM3_DEFAULT_LEVELS 3
#define LSM3_DEFAULT_LEVEL_SIZE_RATIO 4
#define LSM3_MAX_COMPONENTS (LSM3_NUM_TOP_INDEXES + LSM3_MAX_LEVELS + 1)

/*
 * Control structure for Lsm3 index located in shared memory
 */
typedef struct
{
	Oid base;   /* Oid of base index */
	Oid heap;   /* Oid of indexed relation */

	/*
	 * LSM3 MULTI-LEVEL CHANGE:
	 * top[] is still the two-entry mutable L0 buffer; level[] stores immutable/intermediate levels up to LSM3_MAX_LEVELS.
	 */
	Oid top[LSM3_NUM_TOP_INDEXES]; /* Oids of two top indexes */
	Oid level[LSM3_MAX_LEVELS];    /* Oids of intermediate LSM level indexes */

	int access_count[LSM3_NUM_TOP_INDEXES]; /* Access counter for top indexes */
	int active_index; /* Index used for insert */

	/*
	 * LSM3 MULTI-LEVEL CHANGE:
	 * num_levels is the runtime number of active intermediate levels; level_size_ratio is for later compaction thresholds.
	 */
	int num_levels;
	int level_size_ratio;

	uint64 n_merges;  /* Number of performed merges since database open */
	uint64 n_inserts; /* Number of performed inserts since database open  */
	volatile bool start_merge; /* Start merging of top index with base index */
	volatile bool merge_in_progress; /* Overflow of top index intiate merge process */
	PGPROC* merger;   /* Merger background worker */
	Oid     db_id;    /* user ID (for background worker) */
	Oid     user_id;  /* database Id (for background worker) */
	Oid     am_id;    /* Lsm3 AM Oid */
	int     top_index_size; /* Size of top index */
	slock_t spinlock; /* Spinlock to synchronize access */
} Lsm3DictEntry;

/*
 * Opaque part of index scan descriptor
 */
typedef struct
{
	Lsm3DictEntry* entry;      /* Lsm3 control structure */

	/*
	 * LSM3 MULTI-LEVEL CHANGE:
	 * component_index/scan/eof are now sized for top0, top1, level0..levelN, and base.
	 * ncomponents tells the scan path how many of these fixed-capacity slots are active.
	 */
	Relation 	   component_index[LSM3_MAX_COMPONENTS]; /* Opened auxiliary index relations; base uses scan relation itself */
	SortSupport    sortKeys;   /* Context for comparing index tuples */
	IndexScanDesc  scan[LSM3_MAX_COMPONENTS];    /* Scan descrip level_size_ratio is for later compaction thresholdtors for all active components */
	bool           eof[LSM3_MAX_COMPONENTS];     /* Indicators that end of index was reached */
	int            ncomponents; /* Number of active scan components: two tops + active levels + base */

	bool           unique;     /* Whether index is "unique" and we can stop scan after locating first occurrence */
	int            curr_index; /* Index from which last tuple was selected (or -1 if none) */
} Lsm3ScanOpaque;

/* Lsm3 index options */
typedef struct
{
	BTOptions   nbt_opts;       /* Standard B-Tree options */
	int         top_index_size; /* Size of top index (overrode lsm3.top_index_size GUC */

	/*
	 * LSM3 MULTI-LEVEL CHANGE:
	 * These reloptions make the number of levels configurable while preserving fixed shared-memory capacity.
	 */
	int         num_levels;
	int         level_size_ratio;

	bool        unique;			/* Index may not contain duplicates. We prohibit unique constraint for Lsm3 index
                                 * because it can not be enforced. But presence of this index option allows to optimize
								 * index lookup: if key is found in active top index, do not search other two indexes.
                                 */
} Lsm3Options;
