#include <reent.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#include "projdefs.h"
#include "portable.h"
#include "trace.h"
#include "sys/time.h"
#include "sys/times.h"
#include "ff.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

void *byte_memcpy(void *dest, const void *src, size_t n) {
    int i;
    void *_dest = dest;
    for(i = 0; i < n; i++) {
        *(uint8_t*)dest++ = *(uint8_t*)src++;
    }
    return _dest;
}

//#define MALLOC_LICK_SEMAPHORE

#ifdef MALLOC_LICK_SEMAPHORE
static xSemaphoreHandle malloc_lock_mutex;
static unsigned int malloc_lock_ref_count = 0;
#endif

void malloc_lock_init() {
#ifdef MALLOC_LICK_SEMAPHORE
    malloc_lock_mutex = xSemaphoreCreateRecursiveMutex();
#endif
}

void __malloc_lock (struct _reent *reent) {
#ifdef MALLOC_LICK_SEMAPHORE
    if (malloc_lock_mutex == NULL)
        return;

    xTaskHandle task = xTaskGetCurrentTaskHandle();
    //TRACE_INFO(">>>malloc_lock %x %x %u\r\n", task, *((uint32_t*)malloc_lock_mutex + 1), malloc_lock_ref_count);
    if(task) {
        xSemaphoreTakeRecursive(malloc_lock_mutex, -1);
        
    }
    malloc_lock_ref_count++;
    //TRACE_INFO("<<<malloc_lock %x %u\r\n", task, malloc_lock_ref_count);
#else
    //vPortEnterCritical();
    vTaskSuspendAll();
#endif
}

void __malloc_unlock (struct _reent *reent) {
#ifdef MALLOC_LICK_SEMAPHORE
    if (malloc_lock_mutex == NULL)
        return;

    xTaskHandle task = xTaskGetCurrentTaskHandle();
    //TRACE_INFO(">>>malloc_unlock %x %x %u\r\n", task, *((uint32_t*)malloc_lock_mutex + 1), malloc_lock_ref_count);
    malloc_lock_ref_count--;
    if(task) {
        xSemaphoreGiveRecursive(malloc_lock_mutex);
    }
    //TRACE_INFO("<<<malloc_unlock %x %u\r\n", task, malloc_lock_ref_count);
#else
    //vPortExitCritical();
    xTaskResumeAll();
#endif
}

/************************** _sbrk_r *************************************
  Support function. Adjusts end of heap to provide more memory to
  memory allocator. Simple and dumb with no sanity checks.

  This was originally added to provide floating point support
  in printf and friends.

  struct _reent *r -- re-entrancy structure, used by newlib to
  support multiple threads of operation.
  ptrdiff_t nbytes -- number of bytes to add.
  Returns pointer to start of new heap area.

Note:  This implementation is not thread safe (despite taking a
_reent structure as a parameter).
Since _s_r is not used in the current implementation, 
the following messages must be suppressed.

Provided by Uli Hartmann.

References:
http://www.embedded.com/columns/15201376?_requestid=243242
http://forum.sparkfun.com/viewtopic.php?t=5390&postdays=0&postorder=asc&start=0
http://www.siwawi.arubi.uni-kl.de/avr_projects/arm_projects/index_at91.html
*/

/*
   'end' is set in the linker command file and is the end of statically
   allocated data (thus start of heap).
   */
extern int errno;
extern char end[];
extern char __heap_end__[];
static char *heap_ptr; // Points to current end of the heap

void* _sbrk_r(struct _reent *r, ptrdiff_t nbytes)
//void* _sbrk(ptrdiff_t nbytes)
{
    //(void)r;
    char *base;         //  errno should be set to  ENOMEM on error

    TRACE_SYS("_sbrk %d\r\n", nbytes);
    if (!heap_ptr)      //  Initialize if first time through.
        heap_ptr = end;

    if (heap_ptr + nbytes > __heap_end__) {
        //errno = ENOMEM;
        r->_errno = ENOMEM;
        return -1;
    }
    base = heap_ptr;    //  Point to end of heap.
    heap_ptr += nbytes; //  Increase heap.
    return base;        //  Return pointer to start of new heap area.
}

//
//  $Id: syscalls.c 351 2009-01-12 03:05:14Z jcw $
//  $Revision: 351 $
//  $Author: jcw $
//  $Date: 2009-01-11 22:05:14 -0500 (Sun, 11 Jan 2009) $
//  $HeadURL: http://tinymicros.com/svn_public/arm/lpc2148_demo/trunk/newlib/syscalls.c $
//

//
//  Support files for GNU libc.  Files in the system namespace go here.
//  Files in the C namespace (ie those that do not start with an underscore) go in .c
//  
/*
#include "FreeRTOS.h"
#include "task.h"

#include <_ansi.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <sys/times.h>
#include <errno.h>
#include <reent.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>

#include "../uart/uart0.h"
#include "../uart/uart1.h"
#include "../usbser/usbser.h"
#include "../rtc/rtc.h"
#include "../fatfs/ff.h"
#ifdef CFG_TELNETD
#include "../uip/apps/telnetd/telnetd.h"
#endif

//
//  Forward prototypes
//
int     _mkdir        _PARAMS ((const char *, mode_t));
int     _chmod        _PARAMS ((const char *, mode_t));
int     _read         _PARAMS ((int, char *, int));
int     _lseek        _PARAMS ((int, int, int));
int     _write        _PARAMS ((int, const char *, int));
int     _open         _PARAMS ((const char *, int, ...));
int     _close        _PARAMS ((int));
int     _kill         _PARAMS ((int, int));
void    _exit         _PARAMS ((int));
int     _getpid       _PARAMS ((int));
caddr_t _sbrk         _PARAMS ((int));
int     _fstat        _PARAMS ((int, struct stat *));
int     _stat         _PARAMS ((const char *, struct stat *));
int     _link         _PARAMS ((void));
int     _unlink       _PARAMS ((const char *));
void    _raise        _PARAMS ((void));
int     _gettimeofday _PARAMS ((struct timeval *, struct timezone *));
clock_t _times        _PARAMS ((struct tms *));
//int     isatty        _PARAMS ((int));
int     _isatty        _PARAMS ((int));
int     _rename       _PARAMS ((const char *, const char *));
int     _system       _PARAMS ((const char *));
void    _sync         _PARAMS ((void));
void    syscallsInit  _PARAMS ((void));

int     rename        _PARAMS ((const char *, const char *));

static int  set_errno             _PARAMS ((int));
static int  remap_handle          _PARAMS ((int));
static int  findslot              _PARAMS ((int));

*/
//
//  Register name faking - works in collusion with the linker
//
register char *stack_ptr asm ("sp");

//
//  Following is copied from libc/stdio/local.h to check std streams 
//
extern void _EXFUN (__sinit,( struct _reent *));

#define CHECK_INIT(ptr)            \
    do                                 \
{                                  \
    if ((ptr) && !(ptr)->__sdidinit) \
    __sinit (ptr);                   \
}                                  \
while (0)

//
//  Adjust our internal handles to stay away from std* handles
//
#define FILE_HANDLE_OFFSET (0x20)

#define MONITOR_STDIN  0
#define MONITOR_STDOUT 1
#define MONITOR_STDERR 2
#define MONITOR_DBG    3
#define MONITOR_UART0  4
#define MONITOR_UART1  5
#define MONITOR_USB    6
#define MONITOR_FATFS  7

// 
//
//
typedef struct
{
    int handle;
    int pos;
    int flags;
    FIL *fatfsFCB;
}
openFiles_t;

//
//
//
#define MAX_OPEN_FILES 10
static openFiles_t openfiles [MAX_OPEN_FILES];

static int findslot (int fh)
{
    static int slot;
    static int lastfh = -1;

    if ((fh != -1) && (fh == lastfh))
        return slot;

    for (slot = 0; slot < MAX_OPEN_FILES; slot++)
        if (openfiles [slot].handle == fh)
            break;

    lastfh = fh;

    return slot;
}

//
//  Function to convert std(in|out|err) handles to internal versions.  
//
static int remap_handle (int fh)
{
    CHECK_INIT(_REENT);

    if (fh == STDIN_FILENO)
        return MONITOR_STDIN;
    if (fh == STDOUT_FILENO)
        return MONITOR_STDOUT;
    if (fh == STDERR_FILENO)
        return MONITOR_STDERR;

    TRACE_SYS("remap_handle %d\r\n", fh);
    return fh - FILE_HANDLE_OFFSET;
}

#ifdef CFG_FATFS
/* TODO
   FR_OK = 0,
   FR_DISK_ERR,
   FR_INT_ERR,
   FR_NOT_READY,
   FR_NO_FILE,
   FR_NO_PATH,
   FR_INVALID_NAME,
   FR_DENIED,
   FR_EXIST,
   FR_INVALID_OBJECT,
   FR_WRITE_PROTECTED,
   FR_INVALID_DRIVE,
   FR_NOT_ENABLED,
   FR_NO_FILESYSTEM,
   FR_MKFS_ABORTED,
   FR_TIMEOUT
   */
static int remap_fatfs_errors (FRESULT f)
{
    switch (f)
    {
        case FR_NO_FILE         : errno = ENOENT;   break;
        case FR_NO_PATH         : errno = ENOENT;   break;
        case FR_INVALID_NAME    : errno = EINVAL;   break;
        case FR_INVALID_DRIVE   : errno = ENODEV;   break;
        case FR_DENIED          : errno = EACCES;   break;
        case FR_EXIST           : errno = EEXIST;   break;
        case FR_NOT_READY       : errno = EIO;      break;
        case FR_WRITE_PROTECTED : errno = EACCES;   break;
                                  //case FR_RW_ERROR        : errno = EIO;      break;
        case FR_NOT_ENABLED     : errno = EIO;      break;
        case FR_NO_FILESYSTEM   : errno = EIO;      break;
        case FR_INVALID_OBJECT  : errno = EBADF;    break;
        default                 : errno = EIO;      break;
    }

    return -1;
}
#endif

#ifdef CFG_FATFS
static time_t fatfs_time_to_timet (FILINFO *f)
{
    struct tm tm;

    tm.tm_sec  =  (f->ftime & 0x001f) << 1;
    tm.tm_min  =  (f->ftime & 0x07e0) >> 5;
    tm.tm_hour =  (f->ftime & 0xf800) >> 11;
    tm.tm_mday =  (f->fdate & 0x001f);
    tm.tm_mon  = ((f->fdate & 0x01e0) >> 5) - 1;
    tm.tm_year = ((f->fdate & 0xfe00) >> 9) + 80;
    tm.tm_isdst = 0;

    return mktime (&tm);
}
#endif

static int set_errno (int errval)
{
    errno = errval;

    return -1;
}

void syscallsInit (void)
{
    int slot;
    static int initialized = 0;

    if (initialized)
        return;

    initialized = 1;

    __builtin_memset (openfiles, 0, sizeof (openfiles));

    for (slot = 0; slot < MAX_OPEN_FILES; slot++)
        openfiles [slot].handle = -1;

    openfiles [0].handle = MONITOR_STDIN;
    openfiles [1].handle = MONITOR_STDOUT;
    openfiles [2].handle = MONITOR_STDERR;
}

int _mkdir (const char *path, mode_t mode __attribute__ ((unused)))
{
#ifdef CFG_FATFS
    FRESULT f;

    if ((f = f_mkdir (path)) != FR_OK)
        return remap_fatfs_errors (f);

    return 0;
#else
    path = path;
    return set_errno (EIO);
#endif
}

int _chmod (const char *path, mode_t mode)
{
#ifdef CFG_FATFS
    FRESULT f;

    if ((f = f_chmod (path, (mode & S_IWUSR) ? 0 : AM_RDO, AM_RDO)) != FR_OK)
        return remap_fatfs_errors (f);

    return 0;
#else
    path = path;
    mode = mode;
    return set_errno (EIO);
#endif
}

//_ssize_t _read_r (struct _reent *r, int fd, void *ptr, size_t len)
_ssize_t _read (int fd, void *ptr, size_t len)
{
    int i;
    int fh;
    int slot;
    portTickType xBlockTime;
    int bytesUnRead = -1;

    TRACE_SYS("_read %x %x %d\r\n", fd, ptr, len);
    if ((slot = findslot (fh = remap_handle (fd))) == MAX_OPEN_FILES)
        return set_errno (EBADF);

    if (openfiles [slot].flags & O_WRONLY)
        return set_errno (EBADF);

    xBlockTime = (openfiles [slot].flags & O_NONBLOCK) ? 0 : portMAX_DELAY;

    TRACE_SYS("fh %d\r\n", fh);
    switch (fh)
    {
        case MONITOR_STDIN :
            {
                /*
                   for (i = 0; i < len; i++)
                   if (!uart0GetChar ((signed portCHAR *) ptr++, xBlockTime))
                   break;

                   bytesUnRead = len - i;
                   */
                int bytesRead = UsbSerial_read(ptr, len, xBlockTime);
                bytesUnRead = len - bytesRead;
            }
            break;

        case MONITOR_STDOUT :
        case MONITOR_STDERR :
        case MONITOR_DBG :
            break;

        case MONITOR_UART0 :
            {
                /*
                   for (i = 0; i < len; i++)
                   if (!uart0GetChar ((signed portCHAR *) ptr++, xBlockTime))
                   break;

                   bytesUnRead = len - i;
                   */
            }
            break;

        case MONITOR_UART1 :
            {
                /*
                   for (i = 0; i < len; i++)
                   if (!uart1GetChar ((signed portCHAR *) ptr++, xBlockTime))
                   break;

                   bytesUnRead = len - i;
                   */
            }
            break;

        case MONITOR_USB :
            {
#ifdef CFG_USB_SER
                /*
                   for (i = 0; i < len; i++)
                   if (!usbserGetChar ((signed portCHAR *) ptr++, xBlockTime))
                   break;

                   bytesUnRead = len - i;
                   */
                int bytesRead = UsbSerial_read(ptr, len, xBlockTime);
                bytesUnRead = len - bytesRead;
#else
                bytesUnRead = len;
#endif
            }
            break;

        default :
            {
#ifdef CFG_FATFS
                if (openfiles [slot].fatfsFCB)
                {
                    FRESULT f;
                    UINT fatfsBytesRead;

                    if ((f = f_read (openfiles [slot].fatfsFCB, ptr, len, &fatfsBytesRead)) != FR_OK)
                        return remap_fatfs_errors (f);

                    bytesUnRead = len - fatfsBytesRead;
                }
#else
                return set_errno (EIO);
#endif
            }      
            break;
    }

    if (bytesUnRead < 0)
        return -1;

    openfiles [slot].pos += len - bytesUnRead;

    return len - bytesUnRead;
}

int _lseek (int fd, int ptr, int dir)
{
    int fh;
    int slot;
    FRESULT f = FR_INVALID_OBJECT;

    if (((slot = findslot (fh = remap_handle (fd))) == MAX_OPEN_FILES) || !openfiles [slot].fatfsFCB)
        return set_errno (EBADF);

#ifdef CFG_FATFS
    if (dir == SEEK_SET)
        f = f_lseek (openfiles [slot].fatfsFCB, ptr); 
    else if (dir == SEEK_CUR)
        f = f_lseek (openfiles [slot].fatfsFCB, openfiles [slot].fatfsFCB->fptr + ptr); 
    else if (dir == SEEK_END)
        f = f_lseek (openfiles [slot].fatfsFCB, openfiles [slot].fatfsFCB->fsize + ptr); 

    if (f != FR_OK)
        return remap_fatfs_errors (f);

    return openfiles [slot].pos = openfiles [slot].fatfsFCB->fptr;
#else
    ptr = ptr;
    dir = dir;
    f = f;
    return set_errno (EIO);
#endif
}

//_ssize_t _write_r (struct _reent *r, int fd, const void *ptr, size_t len)
_ssize_t _write (int fd, const void *ptr, size_t len)
{
    int i;
    int fh;
    int slot;
    portTickType xBlockTime;
    int bytesUnWritten = -1;

    TRACE_SYS("_write %x %x %d\r\n", fd, ptr, len);
    if ((slot = findslot (fh = remap_handle (fd))) == MAX_OPEN_FILES) {
        TRACE_ERROR("MAX_OPEN_FILES\r\n");
        return set_errno (EBADF);
    }

    if (openfiles [slot].flags & O_RDONLY) {
        TRACE_ERROR("O_RDONLY\r\n");
        return set_errno (EBADF);
    }

    xBlockTime = (openfiles [slot].flags & O_NONBLOCK) ? 0 : portMAX_DELAY;

    TRACE_SYS("fh %d\r\n", fh);
    switch (fh)
    {
        case MONITOR_STDIN :
            break;

        case MONITOR_STDOUT :
            {
            int bytesWritten = UsbSerial_write(ptr, len, xBlockTime);
            //int bytesWritten = len;
            bytesUnWritten = len - bytesWritten;
            }
            break;
        case MONITOR_STDERR :
        case MONITOR_DBG :
            {
                /*
                   for (i = 0; i < len; i++)
                   {
                   if (*ptr == '\n')
                   {
#ifdef CFG_TELNETD
telnetdPutChar ('\r');
#endif

if (!uart0PutChar ('\r', xBlockTime))
break;
}

#ifdef CFG_TELNETD
telnetdPutChar (*ptr);
#endif

if (!uart0PutChar (*ptr++, xBlockTime))
break;
}

bytesUnWritten = len - i;
*/
            int i;
            char *p = (char*)ptr;
            for(i = 0; i < len; i++) {
                if (*p == '\n') {
                    dbg_usart_putchar('\r');
                }
                dbg_usart_putchar(*p++);
            } 
            bytesUnWritten = 0;
        }
        break;
    case MONITOR_UART0 :
    /*
       {
       for (i = 0; i < len; i++)
       if (!uart0PutChar (*ptr++, xBlockTime))
       break;

       bytesUnWritten = len - i;
       }
       */
    break;

    case MONITOR_UART1 :
    /*
       {
       for (i = 0; i < len; i++)
       {
    #ifdef CFG_CONSOLE_UART1
    if (*ptr == '\n')
    {
    #ifdef CFG_TELNETD
    telnetdPutChar ('\r');
    #endif

    if (!uart1PutChar ('\r', xBlockTime))
    break;
    }

    #ifdef CFG_TELNETD
    telnetdPutChar (*ptr);
    #endif
    #endif

    if (!uart1PutChar (*ptr++, xBlockTime))
    break;
    }

    bytesUnWritten = len - i;
    }
    */
    break;

    case MONITOR_USB :
    {
        /*
           for (i = 0; i < len; i++)
           {
    #ifdef CFG_CONSOLE_USB
    if (*ptr == '\n')
    {
    #ifdef CFG_TELNETD
    telnetdPutChar ('\r');
    #endif

    #ifdef CFG_USB_SER
    if (!usbserPutChar ('\r', xBlockTime))
    break;
    #endif
    }

    #ifdef CFG_TELNETD
    telnetdPutChar (*ptr);
    #endif
    #endif

    #ifdef CFG_USB_SER
    if (!usbserPutChar (*ptr++, xBlockTime))
    break;
    #endif
    }

    bytesUnWritten = len - i;
    */
    int bytesWritten = UsbSerial_write(ptr, len, xBlockTime);
    //int bytesWritten = len;
    bytesUnWritten = len - bytesWritten;
    }
    break;

    default :
    {
    #ifdef CFG_FATFS
        if (openfiles [slot].fatfsFCB)
        {
            FRESULT f;
            UINT fatfsBytesWritten;

            if ((f = f_write (openfiles [slot].fatfsFCB, ptr, len, &fatfsBytesWritten)) != FR_OK)
                return remap_fatfs_errors (f);

            bytesUnWritten = len - fatfsBytesWritten;
        }
        else
    #endif
            return set_errno (EBADF);
    }
    break;
    }

    TRACE_SYS("bytesUnWritten %d\r\n", bytesUnWritten);

    if (bytesUnWritten == -1 || bytesUnWritten == len)
        return -1;

    openfiles [slot].pos += len - bytesUnWritten;

    return len - bytesUnWritten;
}

//int _open_r (struct _reent *r, const char *path, int flags, ...)
//int _open_r (struct _reent *r, const char *path, int flags, int mode)
int _open (const char *path, int flags, int mode)
{
    int fh = 0;
    int slot;

    TRACE_SYS("_open %s %x %x\r\n", path, flags, mode);
    
    if ((slot = findslot (-1)) == MAX_OPEN_FILES)
        return set_errno (ENFILE);

    if (flags & O_APPEND)
        flags &= ~O_TRUNC;

    if (!__builtin_strcmp (path, "/dev/uart0"))
        fh = MONITOR_UART0;
    else if (!__builtin_strcmp (path, "/dev/uart1"))
        fh = MONITOR_UART1;
    else if (!__builtin_strcmp (path, "/dev/usb"))
        fh = MONITOR_USB;
    else
    {
#ifdef CFG_FATFS
        UCHAR fatfsFlags = FA_OPEN_EXISTING;
        FRESULT f;

        //
        // FA_OPEN_EXISTING Opens the file. The function fails if the file is not existing. (Default)
        // FA_OPEN_ALWAYS   Opens the file, if it is existing. If not, the function creates the new file.
        // FA_CREATE_NEW    Creates a new file. The function fails if the file is already existing.
        // FA_CREATE_ALWAYS Creates a new file. If the file is existing, it is truncated and overwritten.
        //
        // O_CREAT If the file does not exist it will be created.
        // O_EXCL  When used with O_CREAT, if the file already exists it is an error and the open() will fail.
        // O_TRUNC If the file already exists and is a regular file and the open mode allows writing (i.e., is O_RDWR or O_WRONLY) it will be truncated to length 0. 
        //
        if (((flags & (O_CREAT | O_TRUNC)) == (O_CREAT | O_TRUNC)) && (flags & (O_RDWR | O_WRONLY)))
            fatfsFlags = FA_CREATE_ALWAYS;
        else if ((flags & (O_CREAT | O_EXCL)) == (O_CREAT | O_EXCL))
            fatfsFlags = FA_OPEN_EXISTING;
        else if ((flags & O_CREAT) == O_CREAT)
            fatfsFlags = FA_OPEN_ALWAYS;
        else if ((flags == O_RDONLY) || (flags == O_WRONLY) || (flags == O_RDWR))
            fatfsFlags = FA_OPEN_EXISTING;
        else
            return set_errno (EINVAL);

        if ((flags & O_ACCMODE) == O_RDONLY)
            fatfsFlags |= FA_READ;
        else if ((flags & O_ACCMODE) == O_WRONLY)
            fatfsFlags |= FA_WRITE;
        else if ((flags & O_ACCMODE) == O_RDWR)
            fatfsFlags |= (FA_READ | FA_WRITE);
        else
            return set_errno (EINVAL);

        fh = -1;
        errno = EIO;

        if (!openfiles [slot].fatfsFCB)
            if ((openfiles [slot].fatfsFCB = __builtin_calloc (1, sizeof (FIL))))
                if ((fh = ((f = f_open (openfiles [slot].fatfsFCB, path, fatfsFlags)) == FR_OK) ? (slot + MONITOR_FATFS) : -1) == -1)
                    remap_fatfs_errors (f);
#else
        fh = -1;
        errno = EIO;
#endif
    }

    if (fh >= 0)
    {
        openfiles [slot].handle = fh;
        openfiles [slot].pos = 0;
        openfiles [slot].flags = flags;

        if (flags & O_APPEND)
        {
            if (f_lseek (openfiles [slot].fatfsFCB, openfiles [slot].fatfsFCB->fsize) != FR_OK)
                fh = -1;
            else
                openfiles [slot].pos = openfiles [slot].fatfsFCB->fptr;
        }
    }

    if ((fh < 0) && openfiles [slot].fatfsFCB)
    {
        free (openfiles [slot].fatfsFCB);
        openfiles [slot].fatfsFCB = NULL;
    }

    return fh >= 0 ? (fh + FILE_HANDLE_OFFSET) : -1;
}

//int _close_r (struct _reent *r, int fd)
int _close (int fd)
{
    int slot;

    TRACE_SYS("_close %x\r\n", fd);

    if ((slot = findslot (remap_handle (fd))) == MAX_OPEN_FILES)
        return set_errno (EBADF);

    openfiles [slot].handle = -1;

#ifdef CFG_FATFS
    if (openfiles [slot].fatfsFCB)
    {
        FRESULT f;

        f = f_close (openfiles [slot].fatfsFCB);
        free (openfiles [slot].fatfsFCB);
        openfiles [slot].fatfsFCB = NULL;

        if (f != FR_OK)
            return remap_fatfs_errors (f);
    }
#endif

    return 0;
}

int _kill (int pid __attribute__ ((unused)), int sig __attribute__ ((unused)))
{
    return set_errno (ENOTSUP);
}

void _exit (int status)
{
    /* There is only one SWI for both _exit and _kill. For _exit, call
       the SWI with the second argument set to -1, an invalid value for
       signum, so that the SWI handler can distinguish the two calls.
Note: The RDI implementation of _kill throws away both its
arguments.  */
    _kill (status, -1);

    while (1);
}

int _getpid (int n)
{
    return 1;
    n = n;
}

/*
   caddr_t _sbrk (int incr)
   {
   extern char end asm ("end");  // Defined by the linker.
   extern unsigned long __heap_max;
   static char *heap_end;
   char *prev_heap_end;
   char *eom;

   if (!heap_end)
   heap_end = & end;

   prev_heap_end = heap_end;

//
//  If FreeRTOS is not running, the stack pointer will be in the SVC region,
//  and therefore greater than the start of the heap (end of .bss).  In this
//  case, we see if the heap allocation will run past the current stack
//  pointer, and if so, return ENOMEM (this does not guarantee subsequent
//  stack operations won't overflow into the heap!).  
//
//  If FreeRTOS is running, then we define the end of the heap to be the
//  start of end of the supervisor stack (since FreeRTOS is running, the
//  system stack, and very little of the supervisor stack space is used).
//
eom = (stack_ptr > & end) ? stack_ptr : (char *) __heap_max;

if (heap_end + incr > eom)
{
errno = ENOMEM;
return (caddr_t) -1;
}

heap_end += incr;

return (caddr_t) prev_heap_end;
}
*/

//
//  Tihs is a problem.  FatFS has no way to go from a file handle back to a file,
//  since file name information isn't stored with the handle.  Tried to think of
//  a good way to handle this, couldn't come up with anything.  For now, it
//  returns ENOSYS (not implemented).
//
int _fstat (int fd __attribute__ ((unused)), struct stat *st __attribute__ ((unused)))
{
    return set_errno (ENOSYS);
}

int _stat (const char *fname, struct stat *st)
{
    TRACE_SYS("_stat %s\r\n", fname);
#ifdef CFG_FATFS
    FRESULT f;
    FILINFO fi;

    // MV
    fi.lfname = NULL;
    
    if ((f = f_stat (fname, &fi)) != FR_OK)
        return remap_fatfs_errors (f);

    __builtin_memset (st, 0, sizeof (* st));

    st->st_mode |= (fi.fattrib & AM_DIR) ? S_IFDIR : S_IFREG;
    st->st_mode |= (fi.fattrib & AM_RDO) ? ((S_IRWXU & ~S_IWUSR) | (S_IRWXG & ~S_IWGRP) | (S_IRWXO & ~S_IWOTH)) : (S_IRWXU | S_IRWXG | S_IRWXO);
    st->st_size = fi.fsize;
    st->st_ctime = fatfs_time_to_timet (&fi);
    st->st_mtime = st->st_ctime;
    st->st_atime = st->st_ctime;
    st->st_blksize = 512;

    return 0;
#else
    fname = fname;
    st = st;
    return set_errno (EIO);
#endif
}

//
//  FatFS and FAT file systems don't support links
//
int _link (void)
{
    return set_errno (ENOSYS);
}

int _unlink (const char *path)
{
#ifdef CFG_FATFS
    FRESULT f;

    if ((f = f_unlink (path)) != FR_OK)
        return remap_fatfs_errors (f);

    return 0;
#else
    path = path;
    return set_errno (EIO);
#endif
}

void _raise (void)
{
    return;
}

int _gettimeofday (struct timeval *tp, struct timezone *tzp)
{
    if (tp)
    {
#ifdef CFG_RTC
        unsigned int milliseconds;

        rtc_open();
        tp->tv_sec = rtc_get_epoch_seconds (&milliseconds);
        rtc_close();

        tp->tv_usec = milliseconds * 1000;
#else
        tp->tv_sec = 0;
        tp->tv_usec = 0;
#endif
    }

    //
    //  Return fixed data for the timezone
    //
    if (tzp)
    {
        tzp->tz_minuteswest = 0;
        tzp->tz_dsttime = 0;
    }

    return 0;
}

//
//  Return a clock that ticks at 100Hz
//
clock_t _times (struct tms *tp)
{
#if CFG_PM
    clock_t timeval = (clock_t) Timer_tick_count ();
#else
    clock_t timeval = (clock_t) xTaskGetTickCount ();
#endif

    if (tp)
    {
        tp->tms_utime  = timeval;
        tp->tms_stime  = 0;
        tp->tms_cutime = 0;
        tp->tms_cstime = 0;
    }

    return timeval;
};

//int isatty (int fd)
int _isatty (int fd)
{
    return (fd <= 2) ? 1 : 0;  /* one of stdin, stdout, stderr */
}

int _system (const char *s)
{
    if (s == NULL)
        return 0;

    return set_errno (ENOSYS);
}

//int _rename_r (struct _reent *r, const char *oldpath, const char *newpath)
int _rename (const char *oldpath, const char *newpath)
{
#ifdef CFG_FATFS
    FRESULT f;

    if ((f = f_rename (oldpath, newpath)))
        return remap_fatfs_errors (f);

    return 0;
#else
    oldpath = oldpath;
    newpath = newpath;
    return set_errno (EIO);
#endif
}

//
//  Default crossdev -t options for ARM newlib doesn't define HAVE_RENAME, so
//  it's trying to use the link/unlink process.  FatFS doesn't support link(),
//  so override the newlib rename() to make it work correctly.
//
int rename (const char *oldpath, const char *newpath)
{
    return _rename (oldpath, newpath);
}

void _sync (void)
{
#ifdef CFG_FATFS
    int slot;

    for (slot = 0; slot < MAX_OPEN_FILES; slot++)
        if (openfiles [slot].fatfsFCB)
            f_sync (openfiles [slot].fatfsFCB);
#endif
}
