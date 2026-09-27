#ifndef __CC_H
#define __CC_H

/*
 * Basic type definitions used by the NXP SDK port.
 *
 * The original NXP SDK headers are designed for IAR. This file provides
 * basic integer types and register access qualifiers for the bare-metal
 * GCC environment.
 */

#define __I     volatile const
#define __O     volatile
#define __IO    volatile

typedef signed char             int8_t;
typedef signed short int        int16_t;
typedef signed int              int32_t;
typedef signed long long int    int64_t;

typedef unsigned char           uint8_t;
typedef unsigned short int      uint16_t;
typedef unsigned int            uint32_t;
typedef unsigned long long int  uint64_t;

typedef signed char             s8;
typedef signed short int        s16;
typedef signed int              s32;
typedef signed long long int    s64;

typedef unsigned char           u8;
typedef unsigned short int      u16;
typedef unsigned int            u32;
typedef unsigned long long int  u64;

#endif /* __CC_H */