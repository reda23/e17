/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */
#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)log_put.c	10.44 (Sleepycat) 11/3/98";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#endif

#include "edb_int.h"
#include "shqueue.h"
#include "edb_page.h"
#include "log.h"
#include "hash.h"
#include "clib_ext.h"
#include "common_ext.h"

static int __log_fill __P((DB_LOG *, DB_LSN *, void *, u_int32_t));
static int __log_flush __P((DB_LOG *, const DB_LSN *));
static int __log_newfd __P((DB_LOG *));
static int __log_putr __P((DB_LOG *, DB_LSN *, const DBT *, u_int32_t));
static int __log_write __P((DB_LOG *, void *, u_int32_t));

/*
 * log_put --
 *	Write a log record.
 */
int
log_put(edblp, lsn, edbt, flags)
	DB_LOG *edblp;
	DB_LSN *lsn;
	const DBT *edbt;
	u_int32_t flags;
{
	int ret;

	LOG_PANIC_CHECK(edblp);

	/* Validate arguments. */
	if (flags != 0 && flags != DB_CHECKPOINT &&
	    flags != DB_CURLSN && flags != DB_FLUSH)
		return (__edb_ferr(edblp->edbenv, "log_put", 0));

	LOCK_LOGREGION(edblp);
	ret = __log_put(edblp, lsn, edbt, flags);
	UNLOCK_LOGREGION(edblp);
	return (ret);
}

/*
 * __log_put --
 *	Write a log record; internal version.
 *
 * PUBLIC: int __log_put __P((DB_LOG *, DB_LSN *, const DBT *, u_int32_t));
 */
int
__log_put(edblp, lsn, edbt, flags)
	DB_LOG *edblp;
	DB_LSN *lsn;
	const DBT *edbt;
	u_int32_t flags;
{
	DBT fid_edbt, t;
	DB_LSN r_unused;
	FNAME *fnp;
	LOG *lp;
	u_int32_t lastoff;
	int ret;

	lp = edblp->lp;

	/*
	 * If the application just wants to know where we are, fill in
	 * the information.  Currently used by the transaction manager
	 * to avoid writing TXN_begin records.
	 */
	if (flags == DB_CURLSN) {
		lsn->file = lp->lsn.file;
		lsn->offset = lp->lsn.offset;
		return (0);
	}

	/* If this information won't fit in the file, swap files. */
	if (lp->lsn.offset + sizeof(HDR) + edbt->size > lp->persist.lg_max) {
		if (sizeof(HDR) +
		    sizeof(LOGP) + edbt->size > lp->persist.lg_max) {
			__edb_err(edblp->edbenv,
			    "log_put: record larger than maximum file size");
			return (EINVAL);
		}

		/* Flush the log. */
		if ((ret = __log_flush(edblp, NULL)) != 0)
			return (ret);

		/*
		 * Save the last known offset from the previous file, we'll
		 * need it to initialize the persistent header information.
		 */
		lastoff = lp->lsn.offset;

		/* Point the current LSN to the new file. */
		++lp->lsn.file;
		lp->lsn.offset = 0;

		/* Reset the file write offset. */
		lp->w_off = 0;
	} else
		lastoff = 0;

	/* Initialize the LSN information returned to the user. */
	lsn->file = lp->lsn.file;
	lsn->offset = lp->lsn.offset;

	/*
	 * Insert persistent information as the first record in every file.
	 * Note that the previous length is wrong for the very first record
	 * of the log, but that's okay, we check for it during retrieval.
	 */
	if (lp->lsn.offset == 0) {
		t.data = &lp->persist;
		t.size = sizeof(LOGP);
		if ((ret = __log_putr(edblp, lsn,
		    &t, lastoff == 0 ? 0 : lastoff - lp->len)) != 0)
			return (ret);

		/* Update the LSN information returned to the user. */
		lsn->file = lp->lsn.file;
		lsn->offset = lp->lsn.offset;
	}

	/* Write the application's log record. */
	if ((ret = __log_putr(edblp, lsn, edbt, lp->lsn.offset - lp->len)) != 0)
		return (ret);

	/*
	 * On a checkpoint, we:
	 *	Put out the checkpoint record (above).
	 *	Save the LSN of the checkpoint in the shared region.
	 *	Append the set of file name information into the log.
	 */
	if (flags == DB_CHECKPOINT) {
		lp->chkpt_lsn = *lsn;

		for (fnp = SH_TAILQ_FIRST(&edblp->lp->fq, __fname);
		    fnp != NULL; fnp = SH_TAILQ_NEXT(fnp, q, __fname)) {
			if (fnp->ref == 0)	/* Entry not in use. */
				continue;
			memset(&t, 0, sizeof(t));
			t.data = R_ADDR(edblp, fnp->name_off);
			t.size = strlen(t.data) + 1;
			memset(&fid_edbt, 0, sizeof(fid_edbt));
			fid_edbt.data = fnp->ufid;
			fid_edbt.size = DB_FILE_ID_LEN;
			if ((ret = __log_register_log(edblp, NULL, &r_unused, 0,
			    LOG_CHECKPOINT, &t, &fid_edbt, fnp->id, fnp->s_type))
			    != 0)
				return (ret);
		}
	}

	/*
	 * On a checkpoint or when flush is requested, we:
	 *	Flush the current buffer contents to disk.
	 *	Sync the log to disk.
	 */
	if (flags == DB_FLUSH || flags == DB_CHECKPOINT)
		if ((ret = __log_flush(edblp, NULL)) != 0)
			return (ret);

	/*
	 * On a checkpoint, we:
	 *	Save the time the checkpoint was written.
	 *	Reset the bytes written since the last checkpoint.
	 */
	if (flags == DB_CHECKPOINT) {
		(void)time(&lp->chkpt);
		lp->stat.st_wc_bytes = lp->stat.st_wc_mbytes = 0;
	}
	return (0);
}

/*
 * __log_putr --
 *	Actually put a record into the log.
 */
static int
__log_putr(edblp, lsn, edbt, prev)
	DB_LOG *edblp;
	DB_LSN *lsn;
	const DBT *edbt;
	u_int32_t prev;
{
	HDR hdr;
	LOG *lp;
	int ret;

	lp = edblp->lp;

	/*
	 * Initialize the header.  If we just switched files, lsn.offset will
	 * be 0, and what we really want is the offset of the previous record
	 * in the previous file.  Fortunately, prev holds the value we want.
	 */
	hdr.prev = prev;
	hdr.len = sizeof(HDR) + edbt->size;
	hdr.cksum = __ham_func4(edbt->data, edbt->size);

	if ((ret = __log_fill(edblp, lsn, &hdr, sizeof(HDR))) != 0)
		return (ret);
	lp->len = sizeof(HDR);
	lp->lsn.offset += sizeof(HDR);

	if ((ret = __log_fill(edblp, lsn, edbt->data, edbt->size)) != 0)
		return (ret);
	lp->len += edbt->size;
	lp->lsn.offset += edbt->size;
	return (0);
}

/*
 * log_flush --
 *	Write all records less than or equal to the specified LSN.
 */
int
log_flush(edblp, lsn)
	DB_LOG *edblp;
	const DB_LSN *lsn;
{
	int ret;

	LOG_PANIC_CHECK(edblp);

	LOCK_LOGREGION(edblp);
	ret = __log_flush(edblp, lsn);
	UNLOCK_LOGREGION(edblp);
	return (ret);
}

/*
 * __log_flush --
 *	Write all records less than or equal to the specified LSN; internal
 *	version.
 */
static int
__log_flush(edblp, lsn)
	DB_LOG *edblp;
	const DB_LSN *lsn;
{
	DB_LSN t_lsn;
	LOG *lp;
	int current, ret;

	ret = 0;
	lp = edblp->lp;

	/*
	 * If no LSN specified, flush the entire log by setting the flush LSN
	 * to the last LSN written in the log.  Otherwise, check that the LSN
	 * isn't a non-existent record for the log.
	 */
	if (lsn == NULL) {
		t_lsn.file = lp->lsn.file;
		t_lsn.offset = lp->lsn.offset - lp->len;
		lsn = &t_lsn;
	} else
		if (lsn->file > lp->lsn.file ||
		    (lsn->file == lp->lsn.file &&
		    lsn->offset > lp->lsn.offset - lp->len)) {
			__edb_err(edblp->edbenv,
			    "log_flush: LSN past current end-of-log");
			return (EINVAL);
		}

	/*
	 * If the LSN is less than the last-sync'd LSN, we're done.  Note,
	 * the last-sync LSN saved in s_lsn is the LSN of the first byte
	 * we absolutely know has been written to disk, so the test is <=.
	 */
	if (lsn->file < lp->s_lsn.file ||
	    (lsn->file == lp->s_lsn.file && lsn->offset <= lp->s_lsn.offset))
		return (0);

	/*
	 * We may need to write the current buffer.  We have to write the
	 * current buffer if the flush LSN is greater than or equal to the
	 * buffer's starting LSN.
	 */
	current = 0;
	if (lp->b_off != 0 && log_compare(lsn, &lp->f_lsn) >= 0) {
		if ((ret = __log_write(edblp, lp->buf, lp->b_off)) != 0)
			return (ret);

		lp->b_off = 0;
		current = 1;
	}

	/*
	 * It's possible that this thread may never have written to this log
	 * file.  Acquire a file descriptor if we don't already have one.
	 */
	if (edblp->lfname != edblp->lp->lsn.file)
		if ((ret = __log_newfd(edblp)) != 0)
			return (ret);

	/* Sync all writes to disk. */
	if ((ret = __edb_os_fsync(edblp->lfd)) != 0) {
		__edb_panic(edblp->edbenv, ret);
		return (ret);
	}
	++lp->stat.st_scount;

	/*
	 * Set the last-synced LSN, using the LSN of the current buffer.  If
	 * the current buffer was flushed, we know the LSN of the first byte
	 * of the buffer is on disk, otherwise, we only know that the LSN of
	 * the record before the one beginning the current buffer is on disk.
	 *
	 * XXX
	 * Check to make sure that the saved lsn isn't 0 before we go making
	 * this change.  If DB_CHECKPOINT was called before we actually wrote
	 * something, you can end up here without ever having written anything
	 * to a log file, and decrementing either s_lsn.file or s_lsn.offset
	 * will cause much sadness later on.
	 */
	lp->s_lsn = lp->f_lsn;
	if (!current && lp->s_lsn.file != 0)
		if (lp->s_lsn.offset == 0) {
			--lp->s_lsn.file;
			lp->s_lsn.offset = lp->persist.lg_max;
		} else
			--lp->s_lsn.offset;

	return (0);
}

/*
 * __log_fill --
 *	Write information into the log.
 */
static int
__log_fill(edblp, lsn, addr, len)
	DB_LOG *edblp;
	DB_LSN *lsn;
	void *addr;
	u_int32_t len;
{
	LOG *lp;
	u_int32_t nrec;
	size_t nw, remain;
	int ret;

	/* Copy out the data. */
	for (lp = edblp->lp; len > 0;) {
		/*
		 * If we're beginning a new buffer, note the user LSN to which
		 * the first byte of the buffer belongs.  We have to know this
		 * when flushing the buffer so that we know if the in-memory
		 * buffer needs to be flushed.
		 */
		if (lp->b_off == 0)
			lp->f_lsn = *lsn;

		/*
		 * If we're on a buffer boundary and the data is big enough,
		 * copy as many records as we can directly from the data.
		 */
		if (lp->b_off == 0 && len >= sizeof(lp->buf)) {
			nrec = len / sizeof(lp->buf);
			if ((ret = __log_write(edblp,
			    addr, nrec * sizeof(lp->buf))) != 0)
				return (ret);
			addr = (u_int8_t *)addr + nrec * sizeof(lp->buf);
			len -= nrec * sizeof(lp->buf);
			continue;
		}

		/* Figure out how many bytes we can copy this time. */
		remain = sizeof(lp->buf) - lp->b_off;
		nw = remain > len ? len : remain;
		memcpy(lp->buf + lp->b_off, addr, nw);
		addr = (u_int8_t *)addr + nw;
		len -= nw;
		lp->b_off += nw;

		/* If we fill the buffer, flush it. */
		if (lp->b_off == sizeof(lp->buf)) {
			if ((ret =
			    __log_write(edblp, lp->buf, sizeof(lp->buf))) != 0)
				return (ret);
			lp->b_off = 0;
		}
	}
	return (0);
}

/*
 * __log_write --
 *	Write the log buffer to disk.
 */
static int
__log_write(edblp, addr, len)
	DB_LOG *edblp;
	void *addr;
	u_int32_t len;
{
	LOG *lp;
	ssize_t nw;
	int ret;

	/*
	 * If we haven't opened the log file yet or the current one
	 * has changed, acquire a new log file.
	 */
	lp = edblp->lp;
	if (edblp->lfd == -1 || edblp->lfname != lp->lsn.file)
		if ((ret = __log_newfd(edblp)) != 0)
			return (ret);

	/*
	 * Seek to the offset in the file (someone may have written it
	 * since we last did).
	 */
	if ((ret = __edb_os_seek(edblp->lfd, 0, 0, lp->w_off, 0, SEEK_SET)) != 0 ||
	    (ret = __edb_os_write(edblp->lfd, addr, len, &nw)) != 0) {
		__edb_panic(edblp->edbenv, ret);
		return (ret);
	}
	if (nw != (int32_t)len)
		return (EIO);

	/* Reset the buffer offset and update the seek offset. */
	lp->w_off += len;

	/* Update written statistics. */
	if ((lp->stat.st_w_bytes += len) >= MEGABYTE) {
		lp->stat.st_w_bytes -= MEGABYTE;
		++lp->stat.st_w_mbytes;
	}
	if ((lp->stat.st_wc_bytes += len) >= MEGABYTE) {
		lp->stat.st_wc_bytes -= MEGABYTE;
		++lp->stat.st_wc_mbytes;
	}
	++lp->stat.st_wcount;

	return (0);
}

/*
 * log_file --
 *	Map a DB_LSN to a file name.
 */
int
log_file(edblp, lsn, namep, len)
	DB_LOG *edblp;
	const DB_LSN *lsn;
	char *namep;
	size_t len;
{
	int ret;
	char *name;

	LOG_PANIC_CHECK(edblp);

	LOCK_LOGREGION(edblp);
	ret = __log_name(edblp, lsn->file, &name, NULL, 0);
	UNLOCK_LOGREGION(edblp);
	if (ret != 0)
		return (ret);

	/* Check to make sure there's enough room and copy the name. */
	if (len < strlen(name) + 1) {
		*namep = '\0';
		return (ENOMEM);
	}
	(void)strcpy(namep, name);
	__edb_os_freestr(name);

	return (0);
}

/*
 * __log_newfd --
 *	Acquire a file descriptor for the current log file.
 */
static int
__log_newfd(edblp)
	DB_LOG *edblp;
{
	int ret;
	char *name;

	/* Close any previous file descriptor. */
	if (edblp->lfd != -1) {
		(void)__edb_os_close(edblp->lfd);
		edblp->lfd = -1;
	}

	/* Get the path of the new file and open it. */
	edblp->lfname = edblp->lp->lsn.file;
	if ((ret = __log_name(edblp,
	    edblp->lfname, &name, &edblp->lfd, DB_CREATE | DB_SEQUENTIAL)) != 0)
		__edb_err(edblp->edbenv, "log_put: %s: %s", name, strerror(ret));

	__edb_os_freestr(name);
	return (ret);
}

/*
 * __log_name --
 *	Return the log name for a particular file, and optionally open it.
 *
 * PUBLIC: int __log_name __P((DB_LOG *, u_int32_t, char **, int *, u_int32_t));
 */
int
__log_name(edblp, filenumber, namep, fdp, flags)
	DB_LOG *edblp;
	u_int32_t filenumber, flags;
	char **namep;
	int *fdp;
{
	int ret;
	char *oname;
	char old[sizeof(LFPREFIX) + 5 + 20], new[sizeof(LFPREFIX) + 10 + 20];

	/*
	 * !!!
	 * The semantics of this routine are bizarre.
	 *
	 * The reason for all of this is that we need a place where we can
	 * intercept requests for log files, and, if appropriate, check for
	 * both the old-style and new-style log file names.  The trick is
	 * that all callers of this routine that are opening the log file
	 * read-only want to use an old-style file name if they can't find
	 * a match using a new-style name.  The only down-side is that some
	 * callers may check for the old-style when they really don't need
	 * to, but that shouldn't mess up anything, and we only check for
	 * the old-style name when we've already failed to find a new-style
	 * one.
	 *
	 * Create a new-style file name, and if we're not going to open the
	 * file, return regardless.
	 */
	(void)snprintf(new, sizeof(new), LFNAME, filenumber);
	if ((ret = __edb_appname(edblp->edbenv,
	    DB_APP_LOG, edblp->dir, new, 0, NULL, namep)) != 0 || fdp == NULL)
		return (ret);

	/* Open the new-style file -- if we succeed, we're done. */
	if ((ret = __edb_open(*namep,
	    flags, flags, edblp->lp->persist.mode, fdp)) == 0)
		return (0);

	/*
	 * The open failed... if the DB_RDONLY flag isn't set, we're done,
	 * the caller isn't interested in old-style files.
	 */
	if (!LF_ISSET(DB_RDONLY))
		return (ret);

	/* Create an old-style file name. */
	(void)snprintf(old, sizeof(old), LFNAME_V1, filenumber);
	if ((ret = __edb_appname(edblp->edbenv,
	    DB_APP_LOG, edblp->dir, old, 0, NULL, &oname)) != 0)
		goto err;

	/*
	 * Open the old-style file -- if we succeed, we're done.  Free the
	 * space allocated for the new-style name and return the old-style
	 * name to the caller.
	 */
	if ((ret = __edb_open(oname,
	    flags, flags, edblp->lp->persist.mode, fdp)) == 0) {
		__edb_os_freestr(*namep);
		*namep = oname;
		return (0);
	}

	/*
	 * Couldn't find either style of name -- return the new-style name
	 * for the caller's error message.  If it's an old-style name that's
	 * actually missing we're going to confuse the user with the error
	 * message, but that implies that not only were we looking for an
	 * old-style name, but we expected it to exist and we weren't just
	 * looking for any log file.  That's not a likely error.
	 */
err:	__edb_os_freestr(oname);
	return (ret);
}
