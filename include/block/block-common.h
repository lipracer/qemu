/*
 * QEMU System Emulator block driver
 *
 * Copyright (c) 2003 Fabrice Bellard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#ifndef BLOCK_COMMON_H
#define BLOCK_COMMON_H

#include "qapi/qapi-types-block-core.h"
#include "qemu/queue.h"

/*
 * co_wrapper{*}: Function specifiers used by block-coroutine-wrapper.py
 *
 * Function specifiers, which do nothing but mark functions to be
 * generated by scripts/block-coroutine-wrapper.py
 *
 * Usage: read docs/devel/block-coroutine-wrapper.rst
 *
 * There are 4 kind of specifiers:
 * - co_wrapper functions can be called by only non-coroutine context, because
 *   they always generate a new coroutine.
 * - co_wrapper_mixed functions can be called by both coroutine and
 *   non-coroutine context.
 * - co_wrapper_bdrv_rdlock are co_wrapper functions but automatically take and
 *   release the graph rdlock when creating a new coroutine
 * - co_wrapper_mixed_bdrv_rdlock are co_wrapper_mixed functions but
 *   automatically take and release the graph rdlock when creating a new
 *   coroutine.
 *
 * These functions should not be called from a coroutine_fn; instead,
 * call the wrapped function directly.
 */
#define co_wrapper                     no_coroutine_fn
#define co_wrapper_mixed               no_coroutine_fn coroutine_mixed_fn
#define co_wrapper_bdrv_rdlock         no_coroutine_fn
#define co_wrapper_mixed_bdrv_rdlock   no_coroutine_fn coroutine_mixed_fn

/*
 * no_co_wrapper: Function specifier used by block-coroutine-wrapper.py
 *
 * Function specifier which does nothing but mark functions to be generated by
 * scripts/block-coroutine-wrapper.py.
 *
 * A no_co_wrapper function declaration creates a coroutine_fn wrapper around
 * functions that must not be called in coroutine context. It achieves this by
 * scheduling a BH in the bottom half that runs the respective non-coroutine
 * function. The coroutine yields after scheduling the BH and is reentered when
 * the wrapped function returns.
 *
 * A no_co_wrapper_bdrv_rdlock function is a no_co_wrapper function that
 * automatically takes the graph rdlock when calling the wrapped function. In
 * the same way, no_co_wrapper_bdrv_wrlock functions automatically take the
 * graph wrlock.
 */
#define no_co_wrapper
#define no_co_wrapper_bdrv_rdlock
#define no_co_wrapper_bdrv_wrlock

#include "block/blockjob.h"

/* block.c */
typedef struct BlockDriver BlockDriver;
typedef struct BdrvChild BdrvChild;
typedef struct BdrvChildClass BdrvChildClass;

typedef enum BlockZoneOp {
    BLK_ZO_OPEN,
    BLK_ZO_CLOSE,
    BLK_ZO_FINISH,
    BLK_ZO_RESET,
} BlockZoneOp;

typedef enum BlockZoneModel {
    BLK_Z_NONE = 0x0, /* Regular block device */
    BLK_Z_HM = 0x1, /* Host-managed zoned block device */
    BLK_Z_HA = 0x2, /* Host-aware zoned block device */
} BlockZoneModel;

typedef enum BlockZoneState {
    BLK_ZS_NOT_WP = 0x0,
    BLK_ZS_EMPTY = 0x1,
    BLK_ZS_IOPEN = 0x2,
    BLK_ZS_EOPEN = 0x3,
    BLK_ZS_CLOSED = 0x4,
    BLK_ZS_RDONLY = 0xD,
    BLK_ZS_FULL = 0xE,
    BLK_ZS_OFFLINE = 0xF,
} BlockZoneState;

typedef enum BlockZoneType {
    BLK_ZT_CONV = 0x1, /* Conventional random writes supported */
    BLK_ZT_SWR = 0x2, /* Sequential writes required */
    BLK_ZT_SWP = 0x3, /* Sequential writes preferred */
} BlockZoneType;

/*
 * Zone descriptor data structure.
 * Provides information on a zone with all position and size values in bytes.
 */
typedef struct BlockZoneDescriptor {
    uint64_t start;
    uint64_t length;
    uint64_t cap;
    uint64_t wp;
    BlockZoneType type;
    BlockZoneState state;
} BlockZoneDescriptor;

/*
 * Track write pointers of a zone in bytes.
 */
typedef struct BlockZoneWps {
    CoMutex colock;
    uint64_t wp[];
} BlockZoneWps;

typedef struct BlockDriverInfo {
    /* in bytes, 0 if irrelevant */
    int cluster_size;
    /*
     * A fraction of cluster_size, if supported (currently QCOW2 only); if
     * disabled or unsupported, set equal to cluster_size.
     */
    int subcluster_size;
    /* offset at which the VM state can be saved (0 if not possible) */
    int64_t vm_state_offset;
    bool is_dirty;
    /*
     * True if this block driver only supports compressed writes
     */
    bool needs_compressed_writes;
} BlockDriverInfo;

typedef struct BlockFragInfo {
    uint64_t allocated_clusters;
    uint64_t total_clusters;
    uint64_t fragmented_clusters;
    uint64_t compressed_clusters;
} BlockFragInfo;

typedef enum {
    BDRV_REQ_COPY_ON_READ       = 0x1,
    BDRV_REQ_ZERO_WRITE         = 0x2,

    /*
     * The BDRV_REQ_MAY_UNMAP flag is used in write_zeroes requests to indicate
     * that the block driver should unmap (discard) blocks if it is guaranteed
     * that the result will read back as zeroes. The flag is only passed to the
     * driver if the block device is opened with BDRV_O_UNMAP.
     */
    BDRV_REQ_MAY_UNMAP          = 0x4,

    /*
     * An optimization hint when all QEMUIOVector elements are within
     * previously registered bdrv_register_buf() memory ranges.
     *
     * Code that replaces the user's QEMUIOVector elements with bounce buffers
     * must take care to clear this flag.
     */
    BDRV_REQ_REGISTERED_BUF     = 0x8,

    BDRV_REQ_FUA                = 0x10,
    BDRV_REQ_WRITE_COMPRESSED   = 0x20,

    /*
     * Signifies that this write request will not change the visible disk
     * content.
     */
    BDRV_REQ_WRITE_UNCHANGED    = 0x40,

    /*
     * Forces request serialisation. Use only with write requests.
     */
    BDRV_REQ_SERIALISING        = 0x80,

    /*
     * Execute the request only if the operation can be offloaded or otherwise
     * be executed efficiently, but return an error instead of using a slow
     * fallback.
     */
    BDRV_REQ_NO_FALLBACK        = 0x100,

    /*
     * BDRV_REQ_PREFETCH makes sense only in the context of copy-on-read
     * (i.e., together with the BDRV_REQ_COPY_ON_READ flag or when a COR
     * filter is involved), in which case it signals that the COR operation
     * need not read the data into memory (qiov) but only ensure they are
     * copied to the top layer (i.e., that COR operation is done).
     */
    BDRV_REQ_PREFETCH  = 0x200,

    /*
     * If we need to wait for other requests, just fail immediately. Used
     * only together with BDRV_REQ_SERIALISING. Used only with requests aligned
     * to request_alignment (corresponding assertions are in block/io.c).
     */
    BDRV_REQ_NO_WAIT = 0x400,

    /* Mask of valid flags */
    BDRV_REQ_MASK               = 0x7ff,
} BdrvRequestFlags;

#define BDRV_O_NO_SHARE    0x0001 /* don't share permissions */
#define BDRV_O_RDWR        0x0002
#define BDRV_O_RESIZE      0x0004 /* request permission for resizing the node */
#define BDRV_O_SNAPSHOT    0x0008 /* open the file read only and save
                                     writes in a snapshot */
#define BDRV_O_TEMPORARY   0x0010 /* delete the file after use */
#define BDRV_O_NOCACHE     0x0020 /* do not use the host page cache */
#define BDRV_O_NATIVE_AIO  0x0080 /* use native AIO instead of the
                                     thread pool */
#define BDRV_O_NO_BACKING  0x0100 /* don't open the backing file */
#define BDRV_O_NO_FLUSH    0x0200 /* disable flushing on this disk */
#define BDRV_O_COPY_ON_READ 0x0400 /* copy read backing sectors into image */
#define BDRV_O_INACTIVE    0x0800  /* consistency hint for migration handoff */
#define BDRV_O_CHECK       0x1000  /* open solely for consistency check */
#define BDRV_O_ALLOW_RDWR  0x2000  /* allow reopen to change from r/o to r/w */
#define BDRV_O_UNMAP       0x4000  /* execute guest UNMAP/TRIM operations */
#define BDRV_O_PROTOCOL    0x8000  /* if no block driver is explicitly given:
                                      select an appropriate protocol driver,
                                      ignoring the format layer */
#define BDRV_O_NO_IO       0x10000 /* don't initialize for I/O */
#define BDRV_O_AUTO_RDONLY 0x20000 /* degrade to read-only if opening
                                      read-write fails */
#define BDRV_O_IO_URING    0x40000 /* use io_uring instead of the thread pool */

#define BDRV_O_CBW_DISCARD_SOURCE 0x80000 /* for copy-before-write filter */

#define BDRV_O_CACHE_MASK  (BDRV_O_NOCACHE | BDRV_O_NO_FLUSH)


/* Option names of options parsed by the block layer */

#define BDRV_OPT_CACHE_WB       "cache.writeback"
#define BDRV_OPT_CACHE_DIRECT   "cache.direct"
#define BDRV_OPT_CACHE_NO_FLUSH "cache.no-flush"
#define BDRV_OPT_READ_ONLY      "read-only"
#define BDRV_OPT_AUTO_READ_ONLY "auto-read-only"
#define BDRV_OPT_DISCARD        "discard"
#define BDRV_OPT_FORCE_SHARE    "force-share"
#define BDRV_OPT_ACTIVE         "active"


#define BDRV_SECTOR_BITS   9
#define BDRV_SECTOR_SIZE   (1ULL << BDRV_SECTOR_BITS)

/*
 * Get the first most significant bit of wp. If it is zero, then
 * the zone type is SWR.
 */
#define BDRV_ZT_IS_CONV(wp)    (wp & (1ULL << 63))

#define BDRV_REQUEST_MAX_SECTORS MIN_CONST(SIZE_MAX >> BDRV_SECTOR_BITS, \
                                           INT_MAX >> BDRV_SECTOR_BITS)
#define BDRV_REQUEST_MAX_BYTES (BDRV_REQUEST_MAX_SECTORS << BDRV_SECTOR_BITS)

/*
 * We want allow aligning requests and disk length up to any 32bit alignment
 * and don't afraid of overflow.
 * To achieve it, and in the same time use some pretty number as maximum disk
 * size, let's define maximum "length" (a limit for any offset/bytes request and
 * for disk size) to be the greatest power of 2 less than INT64_MAX.
 */
#define BDRV_MAX_ALIGNMENT (1L << 30)
#define BDRV_MAX_LENGTH (QEMU_ALIGN_DOWN(INT64_MAX, BDRV_MAX_ALIGNMENT))

/*
 * Allocation status flags for bdrv_block_status() and friends.
 *
 * Public flags:
 * BDRV_BLOCK_DATA: allocation for data at offset is tied to this layer
 * BDRV_BLOCK_ZERO: offset reads as zero
 * BDRV_BLOCK_OFFSET_VALID: an associated offset exists for accessing raw data
 * BDRV_BLOCK_ALLOCATED: the content of the block is determined by this
 *                       layer rather than any backing, set by block layer
 * BDRV_BLOCK_EOF: the returned pnum covers through end of file for this
 *                 layer, set by block layer
 * BDRV_BLOCK_COMPRESSED: the underlying data is compressed; only valid for
 *                        the formats supporting compression: qcow, qcow2
 *
 * Internal flags:
 * BDRV_BLOCK_RAW: for use by passthrough drivers, such as raw, to request
 *                 that the block layer recompute the answer from the returned
 *                 BDS; must be accompanied by just BDRV_BLOCK_OFFSET_VALID.
 * BDRV_BLOCK_RECURSE: request that the block layer will recursively search for
 *                     zeroes in file child of current block node inside
 *                     returned region. Only valid together with both
 *                     BDRV_BLOCK_DATA and BDRV_BLOCK_OFFSET_VALID. Should not
 *                     appear with BDRV_BLOCK_ZERO.
 *
 * If BDRV_BLOCK_OFFSET_VALID is set, the map parameter represents the
 * host offset within the returned BDS that is allocated for the
 * corresponding raw guest data.  However, whether that offset
 * actually contains data also depends on BDRV_BLOCK_DATA, as follows:
 *
 * DATA ZERO OFFSET_VALID
 *  t    t        t       sectors read as zero, returned file is zero at offset
 *  t    f        t       sectors read as valid from file at offset
 *  f    t        t       sectors preallocated, read as zero, returned file not
 *                        necessarily zero at offset
 *  f    f        t       sectors preallocated but read from backing_hd,
 *                        returned file contains garbage at offset
 *  t    t        f       sectors preallocated, read as zero, unknown offset
 *  t    f        f       sectors read from unknown file or offset
 *  f    t        f       not allocated or unknown offset, read as zero
 *  f    f        f       not allocated or unknown offset, read from backing_hd
 */
#define BDRV_BLOCK_DATA         0x01
#define BDRV_BLOCK_ZERO         0x02
#define BDRV_BLOCK_OFFSET_VALID 0x04
#define BDRV_BLOCK_RAW          0x08
#define BDRV_BLOCK_ALLOCATED    0x10
#define BDRV_BLOCK_EOF          0x20
#define BDRV_BLOCK_RECURSE      0x40
#define BDRV_BLOCK_COMPRESSED   0x80

/*
 * Block status hints: the bitwise-or of these flags emphasize what
 * the caller hopes to learn, and some drivers may be able to give
 * faster answers by doing less work when the hint permits.
 */
#define BDRV_WANT_ZERO          BDRV_BLOCK_ZERO
#define BDRV_WANT_OFFSET_VALID  BDRV_BLOCK_OFFSET_VALID
#define BDRV_WANT_ALLOCATED     BDRV_BLOCK_ALLOCATED
#define BDRV_WANT_PRECISE       (BDRV_WANT_ZERO | BDRV_WANT_OFFSET_VALID | \
                                 BDRV_WANT_OFFSET_VALID)

typedef QTAILQ_HEAD(BlockReopenQueue, BlockReopenQueueEntry) BlockReopenQueue;

typedef struct BDRVReopenState {
    BlockDriverState *bs;
    int flags;
    BlockdevDetectZeroesOptions detect_zeroes;
    bool backing_missing;
    BlockDriverState *old_backing_bs; /* keep pointer for permissions update */
    BlockDriverState *old_file_bs; /* keep pointer for permissions update */
    QDict *options;
    QDict *explicit_options;
    void *opaque;
} BDRVReopenState;

/*
 * Block operation types
 */
typedef enum BlockOpType {
    BLOCK_OP_TYPE_BACKUP_SOURCE,
    BLOCK_OP_TYPE_BACKUP_TARGET,
    BLOCK_OP_TYPE_CHANGE,
    BLOCK_OP_TYPE_COMMIT_SOURCE,
    BLOCK_OP_TYPE_COMMIT_TARGET,
    BLOCK_OP_TYPE_DRIVE_DEL,
    BLOCK_OP_TYPE_EJECT,
    BLOCK_OP_TYPE_EXTERNAL_SNAPSHOT,
    BLOCK_OP_TYPE_INTERNAL_SNAPSHOT,
    BLOCK_OP_TYPE_INTERNAL_SNAPSHOT_DELETE,
    BLOCK_OP_TYPE_MIRROR_SOURCE,
    BLOCK_OP_TYPE_MIRROR_TARGET,
    BLOCK_OP_TYPE_RESIZE,
    BLOCK_OP_TYPE_STREAM,
    BLOCK_OP_TYPE_REPLACE,
    BLOCK_OP_TYPE_MAX,
} BlockOpType;

/* Block node permission constants */
enum {
    /**
     * A user that has the "permission" of consistent reads is guaranteed that
     * their view of the contents of the block device is complete and
     * self-consistent, representing the contents of a disk at a specific
     * point.
     *
     * For most block devices (including their backing files) this is true, but
     * the property cannot be maintained in a few situations like for
     * intermediate nodes of a commit block job.
     */
    BLK_PERM_CONSISTENT_READ    = 0x01,

    /** This permission is required to change the visible disk contents. */
    BLK_PERM_WRITE              = 0x02,

    /**
     * This permission (which is weaker than BLK_PERM_WRITE) is both enough and
     * required for writes to the block node when the caller promises that
     * the visible disk content doesn't change.
     *
     * As the BLK_PERM_WRITE permission is strictly stronger, either is
     * sufficient to perform an unchanging write.
     */
    BLK_PERM_WRITE_UNCHANGED    = 0x04,

    /** This permission is required to change the size of a block node. */
    BLK_PERM_RESIZE             = 0x08,

    /**
     * There was a now-removed bit BLK_PERM_GRAPH_MOD, with value of 0x10. QEMU
     * 6.1 and earlier may still lock the corresponding byte in block/file-posix
     * locking.  So, implementing some new permission should be very careful to
     * not interfere with this old unused thing.
     */

    BLK_PERM_ALL                = 0x0f,

    DEFAULT_PERM_PASSTHROUGH    = BLK_PERM_CONSISTENT_READ
                                 | BLK_PERM_WRITE
                                 | BLK_PERM_WRITE_UNCHANGED
                                 | BLK_PERM_RESIZE,

    DEFAULT_PERM_UNCHANGED      = BLK_PERM_ALL & ~DEFAULT_PERM_PASSTHROUGH,
};

/*
 * Flags that parent nodes assign to child nodes to specify what kind of
 * role(s) they take.
 *
 * At least one of DATA, METADATA, FILTERED, or COW must be set for
 * every child.
 *
 *
 * = Connection with bs->children, bs->file and bs->backing fields =
 *
 * 1. Filters
 *
 * Filter drivers have drv->is_filter = true.
 *
 * Filter node has exactly one FILTERED|PRIMARY child, and may have other
 * children which must not have these bits (one example is the
 * copy-before-write filter, which also has its target DATA child).
 *
 * Filter nodes never have COW children.
 *
 * For most filters, the filtered child is linked in bs->file, bs->backing is
 * NULL.  For some filters (as an exception), it is the other way around; those
 * drivers will have drv->filtered_child_is_backing set to true (see that
 * field’s documentation for what drivers this concerns)
 *
 * 2. "raw" driver (block/raw-format.c)
 *
 * Formally it's not a filter (drv->is_filter = false)
 *
 * bs->backing is always NULL
 *
 * Only has one child, linked in bs->file. Its role is either FILTERED|PRIMARY
 * (like filter) or DATA|PRIMARY depending on options.
 *
 * 3. Other drivers
 *
 * Don't have any FILTERED children.
 *
 * May have at most one COW child. In this case it's linked in bs->backing.
 * Otherwise bs->backing is NULL. COW child is never PRIMARY.
 *
 * May have at most one PRIMARY child. In this case it's linked in bs->file.
 * Otherwise bs->file is NULL.
 *
 * May also have some other children that don't have the PRIMARY or COW bit set.
 */
enum BdrvChildRoleBits {
    /*
     * This child stores data.
     * Any node may have an arbitrary number of such children.
     */
    BDRV_CHILD_DATA         = (1 << 0),

    /*
     * This child stores metadata.
     * Any node may have an arbitrary number of metadata-storing
     * children.
     */
    BDRV_CHILD_METADATA     = (1 << 1),

    /*
     * A child that always presents exactly the same visible data as
     * the parent, e.g. by virtue of the parent forwarding all reads
     * and writes.
     * This flag is mutually exclusive with DATA, METADATA, and COW.
     * Any node may have at most one filtered child at a time.
     */
    BDRV_CHILD_FILTERED     = (1 << 2),

    /*
     * Child from which to read all data that isn't allocated in the
     * parent (i.e., the backing child); such data is copied to the
     * parent through COW (and optionally COR).
     * This field is mutually exclusive with DATA, METADATA, and
     * FILTERED.
     * Any node may have at most one such backing child at a time.
     */
    BDRV_CHILD_COW          = (1 << 3),

    /*
     * The primary child.  For most drivers, this is the child whose
     * filename applies best to the parent node.
     * Any node may have at most one primary child at a time.
     */
    BDRV_CHILD_PRIMARY      = (1 << 4),

    /* Useful combination of flags */
    BDRV_CHILD_IMAGE        = BDRV_CHILD_DATA
                              | BDRV_CHILD_METADATA
                              | BDRV_CHILD_PRIMARY,
};

/* Mask of BdrvChildRoleBits values */
typedef unsigned int BdrvChildRole;

typedef struct BdrvCheckResult {
    int corruptions;
    int leaks;
    int check_errors;
    int corruptions_fixed;
    int leaks_fixed;
    int64_t image_end_offset;
    BlockFragInfo bfi;
} BdrvCheckResult;

typedef enum {
    BDRV_FIX_LEAKS    = 1,
    BDRV_FIX_ERRORS   = 2,
} BdrvCheckMode;

typedef struct BlockSizes {
    uint32_t phys;
    uint32_t log;
} BlockSizes;

typedef struct HDGeometry {
    uint32_t heads;
    uint32_t sectors;
    uint32_t cylinders;
} HDGeometry;

/*
 * Common functions that are neither I/O nor Global State.
 *
 * These functions must never call any function from other categories
 * (I/O, "I/O or GS", Global State) except this one, but can be invoked by
 * all of them.
 */

char *bdrv_perm_names(uint64_t perm);
uint64_t bdrv_qapi_perm_to_blk_perm(BlockPermission qapi_perm);

void bdrv_init_with_whitelist(void);
bool bdrv_uses_whitelist(void);
int bdrv_is_whitelisted(BlockDriver *drv, bool read_only);

int bdrv_parse_aio(const char *mode, int *flags);
int bdrv_parse_cache_mode(const char *mode, int *flags, bool *writethrough);
int bdrv_parse_discard_flags(const char *mode, int *flags);

int path_has_protocol(const char *path);
int path_is_absolute(const char *path);
char *path_combine(const char *base_path, const char *filename);

char *bdrv_get_full_backing_filename_from_filename(const char *backed,
                                                   const char *backing,
                                                   Error **errp);

#endif /* BLOCK_COMMON_H */
