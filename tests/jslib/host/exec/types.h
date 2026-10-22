/* The few Exec types libraries/javascript.h uses, for the engine core's
 * tests on Linux (tests/jslib/run.sh). Same sizes as on AmigaOS. */
#ifndef EXEC_TYPES_H
#define EXEC_TYPES_H
typedef void *APTR;
typedef long LONG;
typedef unsigned long ULONG;
typedef short WORD;
typedef unsigned short UWORD;
typedef signed char BYTE;
typedef unsigned char UBYTE;
typedef unsigned char *STRPTR;
typedef const unsigned char *CONST_STRPTR;
typedef double DOUBLE;
#define VOID void
#define CONST const
#endif
