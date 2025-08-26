#pragma once

/*
 * All relevant information comes from the 'Tool Interface Standard' Elf specification
 */

_Static_assert(sizeof(unsigned char) == 1, "unsigned char has wrong size");

#define EI_NIDENT 16

#define EI_MAG0 0
#define EI_MAG1 1
#define EI_MAG2 2
#define EI_MAG3 3
#define EI_CLASS 4
#define EI_DATA 5
#define EI_VERSION 6
/* only in 64 bit */
#define EI_OSABI 7
#define EI_ABIVERSION 8

#define ET_NONE 0 /* No file type */
#define ET_REL 1 /* Relocatable file */
#define ET_EXEC 2 /* Executable file */
#define ET_DYN 3 /* Shared object file */
#define ET_CORE 4 /* Core file */
#define ET_LOPROC 0xff00 /* processor specific */
#define ET_HIPROC 0xffff /* processor specific */

#define EM_M32 1
#define EM_SPARC 2
#define EM_386 3
#define EM_68K 4
#define EM_88K 5
#define EM_860 7
#define EM_MIPS 8
#define EM_MIPS_RS4_BE 10

#define PT_NULL 0
#define PT_LOAD 1
#define PT_DYNAMIC 2
#define PT_INTERP 3
#define PT_NOTE 4
#define PT_SHLIB 5
#define PT_PHDR 6
