/*********************************  types.h  ********************************
 *           #######
 *           ##  ##
 *           #  ##    ####   #####    #####  ##  ##   #####
 *             ##    ##  ##  ##  ##  ##      ##  ##  ##
 *            ##  #  ######  ##  ##   ####   ##  ##   ####
 *           ##  ##  ##      ##  ##      ##   #####      ##
 *          #######   ####   ##  ##  #####       ##  #####
 *                                           #####
 *          Z-Wave, the wireless language.
 *
 *              Copyright (c) 2007
 *              Zensys A/S
 *              Denmark
 *
 *              All Rights Reserved
 *
 *    This source file is subject to the terms and conditions of the
 *    Zensys Software License Agreement which restricts the manner
 *    in which it may be used.
 *
 *---------------------------------------------------------------------------
 *
 * Description: See below.
 *
 * Author:   Oleg Zadorozhnyy
 *
 * 2007-08-11   OZA     Integrated with Z-Wave environment.
 *
 * Last Changed By:  $Author: oza $
 * Revision:         $Revision: 1.2 $
 * Last Changed:     $Date: 2007/08/19 11:30:20 $
 *
 ****************************************************************************/

/****************************************************************************/
/*                      PRIVATE TYPES and DEFINITIONS                       */
/****************************************************************************/
#ifndef _ZW_TYPES_H_
#define _ZW_TYPES_H_

#define NO_MEM_FUNCTIONS

#define auto
#define code
#define CODE
#define xdata

#ifndef _ASIX_TYPES_H    //To avoid the multiple definitions of BOOL
#ifndef _MSC_VER
typedef enum _BOOL {
	FALSE = 0, TRUE
} BOOL; // Boolean
#endif

#endif  //_ASIX_TYPES_H

#ifdef __ASIX_C51__
#define REENTRANT reentrant
#else
#define REENTRANT
#endif

#ifdef __ASIX_C51__
#define uintptr_t DWORD
#endif

typedef signed char CHAR;
typedef unsigned char UCHAR;
typedef int INT;
typedef unsigned int UINT;
typedef unsigned long ULONG;

typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef signed short INT16;
typedef unsigned long DWORD;
typedef signed long LONG;
typedef float FLOAT;
typedef double DOUBLE;

typedef BYTE * BYTE_P;

typedef unsigned long u32;
typedef unsigned short u16;
typedef long long int INT64;

#define XBYTE  BYTE
#define PBYTE  BYTE        /* External data byte residing in lower 256 byte in xdata */
#define IBYTE  BYTE

#define NO_MEM_FUNCTIONS

//#include "printf.h"
#define ZW_LOG(a,b)

#define BOOL UCHAR
#define BYTE UCHAR

#define REENTRANT

#include <stdint.h>

typedef uint8_t u8_t;
typedef uint16_t u16_t;
typedef uint32_t u32_t;
typedef int32_t s32_t;

#include "include/ZW_typedefs.h"
#include <string.h>

#endif //_ZW_TYPES_H_
