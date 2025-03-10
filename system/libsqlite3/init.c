/*
** 2010 April 7
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
*************************************************************************
**
** This file implements an example of a simple VFS implementation that 
** omits complex features often not required or not possible on embedded
** platforms.  Code is included to buffer writes to the journal file, 
** which can be a significant performance improvement on some embedded
** platforms.
**
** OVERVIEW
**
**   The code in this file implements a minimal SQLite VFS that can be 
**   used on Linux and other posix-like operating systems. The following 
**   system calls are used:
**
**    File-system: access(), unlink(), getcwd()
**    File IO:     open(), read(), write(), fsync(), close(), fstat()
**    Other:       sleep(), usleep(), time()
**
**   The following VFS features are omitted:
**
**     1. File locking. The user must ensure that there is at most one
**        connection to each database when using this VFS. Multiple
**        connections to a single shared-cache count as a single connection
**        for the purposes of the previous statement.
**
**     2. The loading of dynamic extensions (shared libraries).
**
**     3. Temporary files. The user must configure SQLite to use in-memory
**        temp files when using this VFS. The easiest way to do this is to
**        compile with:
**
**          -DSQLITE_TEMP_STORE=3
**
**     4. File truncation. As of version 3.6.24, SQLite may run without
**        a working xTruncate() call, providing the user does not configure
**        SQLite to use "journal_mode=truncate", or use both
**        "journal_mode=persist" and ATTACHed databases.
**
**   It is assumed that the system uses UNIX-like path-names. Specifically,
**   that '/' characters are used to separate path components and that
**   a path-name is a relative path unless it begins with a '/'. And that
**   no UTF-8 encoded paths are greater than 512 bytes in length.
**
** JOURNAL WRITE-BUFFERING
**
**   To commit a transaction to the database, SQLite first writes rollback
**   information into the journal file. This usually consists of 4 steps:
**
**     1. The rollback information is sequentially written into the journal
**        file, starting at the start of the file.
**     2. The journal file is synced to disk.
**     3. A modification is made to the first few bytes of the journal file.
**     4. The journal file is synced to disk again.
**
**   Most of the data is written in step 1 using a series of calls to the
**   VFS xWrite() method. The buffers passed to the xWrite() calls are of
**   various sizes. For example, as of version 3.6.24, when committing a 
**   transaction that modifies 3 pages of a database file that uses 4096 
**   byte pages residing on a media with 512 byte sectors, SQLite makes 
**   eleven calls to the xWrite() method to create the rollback journal, 
**   as follows:
**
**             Write offset | Bytes written
**             ----------------------------
**                        0            512
**                      512              4
**                      516           4096
**                     4612              4
**                     4616              4
**                     4620           4096
**                     8716              4
**                     8720              4
**                     8724           4096
**                    12820              4
**             ++++++++++++SYNC+++++++++++
**                        0             12
**             ++++++++++++SYNC+++++++++++
**
**   On many operating systems, this is an efficient way to write to a file.
**   However, on some embedded systems that do not cache writes in OS 
**   buffers it is much more efficient to write data in blocks that are
**   an integer multiple of the sector-size in size and aligned at the
**   start of a sector.
**
**   To work around this, the code in this file allocates a fixed size
**   buffer of SQLITE_NUTTXVFS_BUFFERSZ using sqlite3_malloc() whenever a 
**   journal file is opened. It uses the buffer to coalesce sequential
**   writes into aligned SQLITE_NUTTXVFS_BUFFERSZ blocks. When SQLite
**   invokes the xSync() method to sync the contents of the file to disk,
**   all accumulated data is written out, even if it does not constitute
**   a complete block. This means the actual IO to create the rollback 
**   journal for the example transaction above is this:
**
**             Write offset | Bytes written
**             ----------------------------
**                        0           8192
**                     8192           4632
**             ++++++++++++SYNC+++++++++++
**                        0             12
**             ++++++++++++SYNC+++++++++++
**
**   Much more efficient if the underlying OS is not caching write 
**   operations.
*/

#include "sqlite3.h"

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/param.h>
#include <unistd.h>
#include <stdbool.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>

static bool initialized = false;

/*
** Size of the write buffer used by journal files in bytes.
*/
#ifndef SQLITE_NUTTXVFS_BUFFERSZ
# define SQLITE_NUTTXVFS_BUFFERSZ 8192
#endif

/*
** The maximum pathname length supported by this VFS.
*/
#define MAXPATHNAME 128

/*
** When using this VFS, the sqlite3_file* handles that SQLite uses are
** actually pointers to instances of type NuttXFile.
*/
typedef struct NuttXFile NuttXFile;
struct NuttXFile {
  sqlite3_file base;              /* Base class. Must be first. */
  int fd;                         /* File descriptor */

  char *aBuffer;                  /* Pointer to malloc'd buffer */
  int nBuffer;                    /* Valid bytes of data in zBuffer */
  sqlite3_int64 iBufferOfst;      /* Offset in file of zBuffer[0] */
};

/*
** Write directly to the file passed as the first argument. Even if the
** file has a write-buffer (NuttXFile.aBuffer), ignore it.
*/
static int nuttxDirectWrite(NuttXFile *p, const void *zBuf, int iAmt,
                           sqlite_int64 iOfst)
{
  off_t ofst;                     /* Return value from lseek() */
  size_t nWrite;                  /* Return value from write() */

  ofst = lseek(p->fd, iOfst, SEEK_SET);
  if (ofst != iOfst)
    {
      return SQLITE_IOERR_WRITE;
    }

  nWrite = write(p->fd, zBuf, iAmt);

  if (nWrite != iAmt)
    {
      return SQLITE_IOERR_WRITE;
    }

  return SQLITE_OK;
}

/*
** Flush the contents of the NuttXFile.aBuffer buffer to disk. This is a
** no-op if this particular file does not have a buffer (i.e. it is not
** a journal file) or if the buffer is currently empty.
*/
static int nuttxFlushBuffer(NuttXFile *p)
{
  int rc = SQLITE_OK;

  if (p->nBuffer)
    {
      rc = nuttxDirectWrite(p, p->aBuffer, p->nBuffer, p->iBufferOfst);
      p->nBuffer = 0;
    }

  return rc;
}

/*
** Close a file.
*/
static int nuttxClose(sqlite3_file *pFile)
{
  int rc;
  NuttXFile *p = (NuttXFile *) pFile;

  rc = nuttxFlushBuffer(p);
  sqlite3_free(p->aBuffer);
  close(p->fd);

  return rc;
}

/*
** Read data from a file.
*/
static int nuttxRead(sqlite3_file *pFile, void *zBuf, int iAmt,
                    sqlite_int64 iOfst)
{
  NuttXFile *p = (NuttXFile *) pFile;
  off_t ofst;                     /* Return value from lseek() */
  int nRead;                      /* Return value from read() */
  struct stat statbuf;
  int rc;                         /* Return code from nuttxFlushBuffer() */

  /* Flush any data in the write buffer to disk in case this operation
  ** is trying to read data the file-region currently cached in the buffer.
  ** It would be possible to detect this case and possibly save an 
  ** unnecessary write here, but in practice SQLite will rarely read from
  ** a journal file when there is data cached in the write-buffer.
  */

  rc = nuttxFlushBuffer(p);
  if (rc != SQLITE_OK)
    {
      return rc;
    }

  ofst = lseek(p->fd, iOfst, SEEK_SET);

  if (ofst != iOfst)
    {
      return SQLITE_IOERR_READ;
    }

  rc = fstat(p->fd, &statbuf);
  if (rc < 0)
    {
      return SQLITE_IOERR_READ;
    }

  if (statbuf.st_size == 0)
    {
      /* CREATE */
      return SQLITE_IOERR_SHORT_READ;
    }

  nRead = read(p->fd, zBuf, iAmt);

  if (nRead == iAmt)
    {
      return SQLITE_OK;
    }
  else if (nRead >= 0)
    {
      if (nRead < iAmt)
        {
          memset(&((char *) zBuf)[nRead], 0, iAmt - nRead);
        }

      return SQLITE_IOERR_SHORT_READ;
    }

  return SQLITE_IOERR_READ;
}

/*
** Write data to a crash-file.
*/
static int nuttxWrite(sqlite3_file *pFile, const void *zBuf, int iAmt,
                     sqlite_int64 iOfst)
{
  NuttXFile *p = (NuttXFile *) pFile;

  if (p->aBuffer)
    {
      char *z = (char *) zBuf;      /* Pointer to remaining data to write */
      int n = iAmt;                 /* Number of bytes at z */
      sqlite3_int64 i = iOfst;      /* File offset to write to */

      while (n > 0)
        {
          int nCopy;                  /* Number of bytes to copy into buffer */

          /* If the buffer is full, or if this data is not being written directly
          ** following the data already buffered, flush the buffer. Flushing
          ** the buffer is a no-op if it is empty.
          */

          if (p->nBuffer == SQLITE_NUTTXVFS_BUFFERSZ ||
              p->iBufferOfst+p->nBuffer != i)
            {
              int rc = nuttxFlushBuffer(p);

              if (rc != SQLITE_OK)
                {
                  return rc;
                }
            }

          p->iBufferOfst = i - p->nBuffer;

          /* Copy as much data as possible into the buffer. */
          nCopy = SQLITE_NUTTXVFS_BUFFERSZ - p->nBuffer;

          if (nCopy > n)
            {
              nCopy = n;
            }

          memcpy(&p->aBuffer[p->nBuffer], z, nCopy);
          p->nBuffer += nCopy;

          n -= nCopy;
          i += nCopy;
          z += nCopy;
        }
    }
  else
    {
      return nuttxDirectWrite(p, zBuf, iAmt, iOfst);
    }

  return SQLITE_OK;
}

/*
** Truncate a file.
*/
static int nuttxTruncate(sqlite3_file *pFile, sqlite_int64 size)
{
  if (ftruncate(((NuttXFile *) pFile)->fd, size))
    {
      return SQLITE_IOERR_TRUNCATE;
    }

  return SQLITE_OK;
}

/*
** Sync the contents of the file to the persistent media.
*/
static int nuttxSync(sqlite3_file *pFile, int flags)
{
  NuttXFile *p = (NuttXFile *) pFile;
  int rc;

  rc = nuttxFlushBuffer(p);

  if (rc != SQLITE_OK)
    {
      return rc;
    }

  rc = fsync(p->fd);
  return (rc == 0 ? SQLITE_OK : SQLITE_IOERR_FSYNC);
}

/*
** Write the size of the file in bytes to *pSize.
*/
static int nuttxFileSize(sqlite3_file *pFile, sqlite_int64 *pSize)
{
  NuttXFile *p = (NuttXFile *) pFile;
  int rc;                         /* Return code from fstat() call */
  struct stat sStat;              /* Output of fstat() call */

  /* Flush the contents of the buffer to disk. As with the flush in the
  ** nuttxRead() method, it would be possible to avoid this and save a write
  ** here and there. But in practice this comes up so infrequently it is
  ** not worth the trouble.
  */
  rc = nuttxFlushBuffer(p);

  if (rc != SQLITE_OK)
    {
      return rc;
    }

  rc = fstat(p->fd, &sStat);

  if (rc != 0)
    {
      return SQLITE_IOERR_FSTAT;
    }

  *pSize = sStat.st_size;
  return SQLITE_OK;
}

/*
** Locking functions. The xLock() and xUnlock() methods are both no-ops.
** The xCheckReservedLock() always indicates that no other process holds
** a reserved lock on the database file. This ensures that if a hot-journal
** file is found in the file-system it is rolled back.
*/
static int nuttxLock(sqlite3_file *pFile, int eLock)
{
  return SQLITE_OK;
}

static int nuttxUnlock(sqlite3_file *pFile, int eLock)
{
  return SQLITE_OK;
}

static int nuttxCheckReservedLock(sqlite3_file *pFile, int *pResOut)
{
  *pResOut = 0;
  return SQLITE_OK;
}

/*
** No xFileControl() verbs are implemented by this VFS.
*/
static int nuttxFileControl(sqlite3_file *pFile, int op, void *pArg)
{
  return SQLITE_NOTFOUND;
}

/*
** The xSectorSize() and xDeviceCharacteristics() methods. These two
** may return special values allowing SQLite to optimize file-system 
** access to some extent. But it is also safe to simply return 0.
*/
static int nuttxSectorSize(sqlite3_file *pFile)
{
  return 0;
}

static int nuttxDeviceCharacteristics(sqlite3_file *pFile)
{
  return 0;
}

/*
** Open a file handle.
*/
static int nuttxOpen(sqlite3_vfs *pVfs, const char *zName, sqlite3_file *pFile,
                    int flags, int *pOutFlags)
{
  static const sqlite3_io_methods nuttxio = {
    1,                             /* iVersion */
    nuttxClose,                    /* xClose */
    nuttxRead,                     /* xRead */
    nuttxWrite,                    /* xWrite */
    nuttxTruncate,                 /* xTruncate */
    nuttxSync,                     /* xSync */
    nuttxFileSize,                 /* xFileSize */
    nuttxLock,                     /* xLock */
    nuttxUnlock,                   /* xUnlock */
    nuttxCheckReservedLock,        /* xCheckReservedLock */
    nuttxFileControl,              /* xFileControl */
    nuttxSectorSize,               /* xSectorSize */
    nuttxDeviceCharacteristics     /* xDeviceCharacteristics */
  };

  NuttXFile *p = (NuttXFile *) pFile; /* Populate this structure */
  int oflags = 0;                   /* flags to pass to open() call */
  char *aBuf = 0;

  if (zName == 0)
    {
      return SQLITE_IOERR;
    }

  if (flags&SQLITE_OPEN_MAIN_JOURNAL)
    {
      aBuf = (char *) sqlite3_malloc(SQLITE_NUTTXVFS_BUFFERSZ);

      if (!aBuf)
        {
          return SQLITE_NOMEM;
        }
    }

  if (flags&SQLITE_OPEN_EXCLUSIVE)
    {
      oflags |= O_EXCL;
    }

  if (flags&SQLITE_OPEN_CREATE)
    {
      oflags |= O_CREAT;
    }

  if (flags&SQLITE_OPEN_READONLY)
    {
      oflags |= O_RDONLY;
    }

  if (flags&SQLITE_OPEN_READWRITE)
    {
      oflags |= O_RDWR;
    }

  memset(p, 0, sizeof(NuttXFile));
  p->fd = open(zName, oflags, 0600);

  if (p->fd < 0)
    {
      sqlite3_free(aBuf);
      return SQLITE_CANTOPEN;
    }

  p->aBuffer = aBuf;

  if (pOutFlags)
    {
      *pOutFlags = flags;
    }

  p->base.pMethods = &nuttxio;
  return SQLITE_OK;
}

/*
** Delete the file identified by argument zPath. If the dirSync parameter
** is non-zero, then ensure the file-system modification to delete the
** file has been synced to disk before returning.
*/
static int nuttxDelete(sqlite3_vfs *pVfs, const char *zPath, int dirSync)
{
  int rc;  /* Return code */

  rc = unlink(zPath);

  if (rc != 0 && errno == ENOENT)
    {
      return SQLITE_OK;
    }

  if (rc == 0 && dirSync)
    {
      int dfd;                      /* File descriptor open on directory */
      char *zSlash;
      char zDir[MAXPATHNAME + 1];   /* Name of directory containing file zPath */

      /* Figure out the directory name from the path of the file deleted. */
      sqlite3_snprintf(MAXPATHNAME, zDir, "%s", zPath);
      zDir[MAXPATHNAME] = '\0';
      zSlash = strrchr(zDir,'/');

      if (zSlash)
        {
          /* Open a file-descriptor on the directory. Sync. Close. */
          zSlash[0] = 0;
          dfd = open(zDir, O_RDONLY, 0);

          if (dfd < 0)
            {
              rc = -1;
            }
          else
            {
              rc = fsync(dfd);
              close(dfd);
            }
        }
    }

  return (rc == 0 ? SQLITE_OK : SQLITE_IOERR_DELETE);
}

#ifndef F_OK
# define F_OK 0
#endif
#ifndef R_OK
# define R_OK 4
#endif
#ifndef W_OK
# define W_OK 2
#endif

/*
** Query the file-system to see if the named file exists, is readable or
** is both readable and writable.
*/
static int nuttxAccess(sqlite3_vfs *pVfs, const char *zPath, int flags,
                      int *pResOut)
{
  int rc;                         /* access() return code */
  int eAccess = F_OK;             /* Second argument to access() */

  if (flags == SQLITE_ACCESS_READWRITE)
    {
      eAccess = R_OK | W_OK;
    }

  if (flags == SQLITE_ACCESS_READ)
    {
      eAccess = R_OK;
    }

  rc = access(zPath, eAccess);
  *pResOut = (rc == 0);

  return SQLITE_OK;
}

/*
** Argument zPath points to a nul-terminated string containing a file path.
** If zPath is an absolute path, then it is copied as is into the output 
** buffer. Otherwise, if it is a relative path, then the equivalent full
** path is written to the output buffer.
**
** This function assumes that paths are UNIX style. Specifically, that:
**
**   1. Path components are separated by a '/'. and 
**   2. Full paths begin with a '/' character.
*/
static int nuttxFullPathname(sqlite3_vfs *pVfs, const char *zPath, int nPathOut,
                            char *zPathOut)
{
  char zDir[MAXPATHNAME + 1];

  if (zPath[0] == '/')
    {
      zDir[0] = '\0';
    }
  else
    {
      if (getcwd(zDir, sizeof(zDir)) == 0)
        {
          return SQLITE_IOERR;
        }
    }

  zDir[MAXPATHNAME] = '\0';

  sqlite3_snprintf(nPathOut, zPathOut, "%s/%s", zDir, zPath);
  zPathOut[nPathOut - 1] = '\0';

  return SQLITE_OK;
}

/*
** The following four VFS methods:
**
**   xDlOpen
**   xDlError
**   xDlSym
**   xDlClose
**
** are supposed to implement the functionality needed by SQLite to load
** extensions compiled as shared objects. This simple VFS does not support
** this functionality, so the following functions are no-ops.
*/
static void *nuttxDlOpen(sqlite3_vfs *pVfs, const char *zPath)
{
  return 0;
}

static void nuttxDlError(sqlite3_vfs *pVfs, int nByte, char *zErrMsg)
{
  sqlite3_snprintf(nByte, zErrMsg, "Loadable extensions are not supported");
  zErrMsg[nByte - 1] = '\0';
}

static void (*nuttxDlSym(sqlite3_vfs *pVfs, void *pH, const char *z))(void)
{
  return 0;
}

static void nuttxDlClose(sqlite3_vfs *pVfs, void *pHandle)
{
  return;
}

/*
** Parameter zByte points to a buffer nByte bytes in size. Populate this
** buffer with pseudo-random data.
*/
static int nuttxRandomness(sqlite3_vfs *pVfs, int nByte, char *zByte)
{
  return SQLITE_OK;
}

/*
** Sleep for at least nMicro microseconds. Return the (approximate) number 
** of microseconds slept for.
*/
static int nuttxSleep(sqlite3_vfs *pVfs, int nMicro)
{
  sleep(nMicro / 1000000);
  usleep(nMicro % 1000000);

  return nMicro;
}

/*
** Set *pTime to the current UTC time expressed as a Julian day. Return
** SQLITE_OK if successful, or an error code otherwise.
**
**   http://en.wikipedia.org/wiki/Julian_day
**
** This implementation is not very good. The current time is rounded to
** an integer number of seconds. Also, assuming time_t is a signed 32-bit 
** value, it will stop working some time in the year 2038 AD (the so-called
** "year 2038" problem that afflicts systems that store time this way). 
*/
static int nuttxCurrentTime(sqlite3_vfs *pVfs, double *pTime)
{
  time_t t = time(0);

  *pTime = t / 86400.0 + 2440587.5;

  return SQLITE_OK;
}

/*
** This function returns a pointer to the VFS implemented in this file.
** To make the VFS available to SQLite:
**
**   sqlite3_vfs_register(sqlite3_nuttxvfs(), 0);
*/
sqlite3_vfs *sqlite3_nuttxvfs(void)
{
  static sqlite3_vfs nuttxvfs = {
    1,                            /* iVersion */
    sizeof(NuttXFile),             /* szOsFile */
    MAXPATHNAME,                  /* mxPathname */
    0,                            /* pNext */
    "nuttx",                       /* zName */
    0,                            /* pAppData */
    nuttxOpen,                     /* xOpen */
    nuttxDelete,                   /* xDelete */
    nuttxAccess,                   /* xAccess */
    nuttxFullPathname,             /* xFullPathname */
    nuttxDlOpen,                   /* xDlOpen */
    nuttxDlError,                  /* xDlError */
    nuttxDlSym,                    /* xDlSym */
    nuttxDlClose,                  /* xDlClose */
    nuttxRandomness,               /* xRandomness */
    nuttxSleep,                    /* xSleep */
    nuttxCurrentTime,              /* xCurrentTime */
  };

  return &nuttxvfs;
}

int sqlite3_os_init(void)
{
  if (!initialized)
    {
      sqlite3_vfs_register(sqlite3_nuttxvfs(), 1);
      initialized = true;
    }

  return SQLITE_OK;
}

int sqlite3_os_end(void){
  return SQLITE_OK;
}
