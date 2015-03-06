#ifndef COFF_H
#define COFF_H

#include <stdint.h>

#define COFF_GET_U8( coff, field ) ( (uint8_t)( ( coff ).field[ 0 ] ) )
#define COFF_GET_U16( coff, field ) ( (uint16_t)( coff ).field[ 0 ] | (uint32_t)( coff ).field[ 1 ] << 8 )
#define COFF_GET_U32( coff, field ) ( (uint32_t)( coff ).field[ 0 ] | (uint32_t)( coff ).field[ 1 ] << 8 | (uint32_t)( coff ).field[ 2 ] << 16 | (uint32_t)( coff ).field[ 3 ] << 24 )
#define COFF_GET_UINT( coff, field ) ( sizeof( ( coff ).field ) == 1 ? COFF_GET_U8( coff, field ) : sizeof( ( coff ).field ) == 2 ? COFF_GET_U16( coff, field ) : COFF_GET_U32( coff, field ) )

#define COFF_GET_S8( coff, field ) ( (int8_t)( ( coff ).field[ 0 ] ) )
#define COFF_GET_S16( coff, field ) ( (int16_t)( (uint32_t)( coff ).field[ 0 ] | (uint32_t)( coff ).field[ 1 ] << 8 ) )
#define COFF_GET_S32( coff, field ) ( (int32_t)( coff ).field[ 0 ] | (uint32_t)( coff ).field[ 1 ] << 8 | (uint32_t)( coff ).field[ 2 ] << 16 | (uint32_t)( coff ).field[ 3 ] << 24 )
#define COFF_GET_INT( coff, field ) ( sizeof( ( coff ).field ) == 1 ? COFF_GET_S8( coff, field ) : sizeof( ( coff ).field ) == 2 ? COFF_GET_S16( coff, field ) : COFF_GET_S32( coff, field ) )

/*
http://msdn.microsoft.com/en-us/windows/hardware/gg463119.aspx
*/

/* 2.3. COFF File Header (Object and Image) */
typedef struct
{
  uint8_t Machine[ 2 ];
  uint8_t NumberOfSections[ 2 ];
  uint8_t TimeDateStamp[ 4 ];
  uint8_t PointerToSymbolTable[ 4 ];
  uint8_t NumberOfSymbols[ 4 ];
  uint8_t SizeOfOptionalHeader[ 2 ];
  uint8_t Characteristics[ 2 ];
}
coff_header_t;

#define COFF_HEADER_SIZE 20

/* 3. Section Table (Section Headers) */
typedef struct
{
  uint8_t Name[ 8 ];
  uint8_t VirtualSize[ 4 ];
  uint8_t VirtualAddress[ 4 ];
  uint8_t SizeOfRawData[ 4 ];
  uint8_t PointerToRawData[ 4 ];
  uint8_t PointerToRelocations[ 4 ];
  uint8_t PointerToLineNumbers[ 4 ];
  uint8_t NumberOfRelocations[ 2 ];
  uint8_t NumberOfLineNumbers[ 2 ];
  uint8_t Characteristics[ 4 ];
}
coff_section_t;

#define COFF_SECTION_SIZE 40

/* 4.2. COFF Relocations (Object Only) */
typedef struct
{
  uint8_t VirtualAddress[ 4 ];
  uint8_t SymbolTableIndex[ 4 ];
  uint8_t Type[ 2 ];
}
coff_relocation_t;

#define COFF_RELOCATION_SIZE 10

/* 4.4. COFF Symbol Table */
typedef struct
{
  union
  {
    uint8_t ShortName[ 8 ];
    
    struct
    {
      uint8_t Zeroes[ 4 ];
      uint8_t Offset[ 4 ];
    }
    LongName;
  }
  Name;
  
  uint8_t Value[ 4 ];
  uint8_t SectionNumber[ 2 ]; /* signed */
  uint8_t Type[ 2 ];
  uint8_t StorageClass[ 1 ];
  uint8_t NumberOfAuxSymbols[ 1 ];
}
coff_symbol_t;

#define COFF_SYMBOL_SIZE 18

/* 2.3.1. Machine Types */
enum
{
  IMAGE_FILE_MACHINE_UNKNOWN = 0x0000, /* The contents of this field are assumed to be applicable to any machine type */
  IMAGE_FILE_MACHINE_AM33 = 0x01d3, /* Matsushita AM33 */
  IMAGE_FILE_MACHINE_AMD64 = 0x8664, /* x64 */
  IMAGE_FILE_MACHINE_ARM = 0x01c0, /* ARM little endian */
  IMAGE_FILE_MACHINE_ARMNT = 0x1c4, /* ARMv7 (or higher) Thumb mode only */
  IMAGE_FILE_MACHINE_ARM64 = 0xaa64, /* ARMv8 in 64-bit mode */
  IMAGE_FILE_MACHINE_EBC = 0x0ebc, /* EFI byte code */
  IMAGE_FILE_MACHINE_I386 = 0x014c, /* Intel 386 or later processors and compatible processors */
  IMAGE_FILE_MACHINE_IA64 = 0x0200, /* Intel Itanium processor family */
  IMAGE_FILE_MACHINE_M32R = 0x9041, /* Mitsubishi M32R little endian */
  IMAGE_FILE_MACHINE_MIPS16 = 0x0266, /* MIPS16 */
  IMAGE_FILE_MACHINE_MIPSFPU = 0x0366, /* MIPS with FPU */
  IMAGE_FILE_MACHINE_MIPSFPU16 = 0x0466, /* MIPS16 with FPU */
  IMAGE_FILE_MACHINE_POWERPC = 0x01f0, /* Power PC little endian */
  IMAGE_FILE_MACHINE_POWERPCFP = 0x01f1, /* Power PC with floating point support */
  IMAGE_FILE_MACHINE_R4000 = 0x0166, /* MIPS little endian */
  IMAGE_FILE_MACHINE_SH3 = 0x01a2, /* Hitachi SH3 */
  IMAGE_FILE_MACHINE_SH3DSP = 0x01a3, /* Hitachi SH3 DSP */
  IMAGE_FILE_MACHINE_SH4 = 0x01a6, /* Hitachi SH4 */
  IMAGE_FILE_MACHINE_SH5 = 0x01a8, /* Hitachi SH5 */
  IMAGE_FILE_MACHINE_THUMB = 0x01c2, /* ARM or Thumb (“interworking”) */
  IMAGE_FILE_MACHINE_WCEMIPSV2 = 0x0169, /* MIPS little-endian WCE v2 */
};

/* 2.3.2. Characteristics */
enum
{
  IMAGE_FILE_RELOCS_STRIPPED = 0x0001,
  IMAGE_FILE_EXECUTABLE_IMAGE = 0x0002,
  IMAGE_FILE_LINE_NUMS_STRIPPED = 0x0004,
  IMAGE_FILE_LOCAL_SYMS_STRIPPED = 0x0008,
  IMAGE_FILE_AGGRESSIVE_WS_TRIM = 0x0010,
  IMAGE_FILE_LARGE_ADDRESS_AWARE = 0x0020,
  /* reserved 0x0040, */
  IMAGE_FILE_BYTES_REVERSED_LO = 0x0080,
  IMAGE_FILE_32BIT_MACHINE = 0x0100,
  IMAGE_FILE_DEBUG_STRIPPED = 0x0200,
  IMAGE_FILE_REMOVABLE_RUN_FROM_SWAP = 0x0400,
  IMAGE_FILE_NET_RUN_FROM_SWAP = 0x0800,
  IMAGE_FILE_SYSTEM = 0x1000,
  IMAGE_FILE_DLL = 0x2000,
  IMAGE_FILE_UP_SYSTEM_ONLY = 0x4000,
  IMAGE_FILE_BYTES_REVERSED_HI = 0x8000,
};

/* 3.1. Section Flags */
enum
{
  /* reserved 0x00000000, */
  /* reserved 0x00000001, */
  /* reserved 0x00000002, */
  /* reserved 0x00000004, */
  IMAGE_SCN_TYPE_NO_PAD = 0x00000008,
  /* reserved 0x00000010, */
  IMAGE_SCN_CNT_CODE = 0x00000020,
  IMAGE_SCN_CNT_INITIALIZED_DATA = 0x00000040,
  IMAGE_SCN_CNT_UNINITIALIZED_DATA = 0x00000080,
  IMAGE_SCN_LNK_OTHER = 0x00000100,
  IMAGE_SCN_LNK_INFO = 0x00000200,
  /* reserved 0x00000400, */
  IMAGE_SCN_LNK_REMOVE = 0x00000800,
  IMAGE_SCN_LNK_COMDAT = 0x00001000,
  IMAGE_SCN_GPREL = 0x00008000,
  IMAGE_SCN_MEM_PURGEABLE = 0x00020000,
  IMAGE_SCN_MEM_16BIT = 0x00020000,
  IMAGE_SCN_MEM_LOCKED = 0x00040000,
  IMAGE_SCN_MEM_PRELOAD = 0x00080000,
  IMAGE_SCN_ALIGN_1BYTES = 0x00100000,
  IMAGE_SCN_ALIGN_2BYTES = 0x00200000,
  IMAGE_SCN_ALIGN_4BYTES = 0x00300000,
  IMAGE_SCN_ALIGN_8BYTES = 0x00400000,
  IMAGE_SCN_ALIGN_16BYTES = 0x00500000,
  IMAGE_SCN_ALIGN_32BYTES = 0x00600000,
  IMAGE_SCN_ALIGN_64BYTES = 0x00700000,
  IMAGE_SCN_ALIGN_128BYTES = 0x00800000,
  IMAGE_SCN_ALIGN_256BYTES = 0x00900000,
  IMAGE_SCN_ALIGN_512BYTES = 0x00A00000,
  IMAGE_SCN_ALIGN_1024BYTES = 0x00B00000,
  IMAGE_SCN_ALIGN_2048BYTES = 0x00C00000,
  IMAGE_SCN_ALIGN_4096BYTES = 0x00D00000,
  IMAGE_SCN_ALIGN_8192BYTES = 0x00E00000,
  IMAGE_SCN_LNK_NRELOC_OVFL = 0x01000000,
  IMAGE_SCN_MEM_DISCARDABLE = 0x02000000,
  IMAGE_SCN_MEM_NOT_CACHED = 0x04000000,
  IMAGE_SCN_MEM_NOT_PAGED = 0x08000000,
  IMAGE_SCN_MEM_SHARED = 0x10000000,
  IMAGE_SCN_MEM_EXECUTE = 0x20000000,
  IMAGE_SCN_MEM_READ = 0x40000000,
  IMAGE_SCN_MEM_WRITE = 0x80000000,
};

/* 4.2.1. Type Indicators */
enum
{
  /* x64 Processors */
  IMAGE_REL_AMD64_ABSOLUTE = 0x0000,
  IMAGE_REL_AMD64_ADDR64 = 0x0001,
  IMAGE_REL_AMD64_ADDR32 = 0x0002,
  IMAGE_REL_AMD64_ADDR32NB = 0x0003,
  IMAGE_REL_AMD64_REL32 = 0x0004,
  IMAGE_REL_AMD64_REL32_1 = 0x0005,
  IMAGE_REL_AMD64_REL32_2 = 0x0006,
  IMAGE_REL_AMD64_REL32_3 = 0x0007,
  IMAGE_REL_AMD64_REL32_4 = 0x0008,
  IMAGE_REL_AMD64_REL32_5 = 0x0009,
  IMAGE_REL_AMD64_SECTION = 0x000A,
  IMAGE_REL_AMD64_SECREL = 0x000B,
  IMAGE_REL_AMD64_SECREL7 = 0x000C,
  IMAGE_REL_AMD64_TOKEN = 0x000D,
  IMAGE_REL_AMD64_SREL32 = 0x000E,
  IMAGE_REL_AMD64_PAIR = 0x000F,
  IMAGE_REL_AMD64_SSPAN32 = 0x0010,
};

/* 4.4.2. Section Number Values */
enum
{
  IMAGE_SYM_UNDEFINED = 0,
  IMAGE_SYM_ABSOLUTE = -1,
  IMAGE_SYM_DEBUG = -2,
};

/* 4.4.3. Type Representation */
enum
{
  /* LSB */
  IMAGE_SYM_TYPE_NULL = 0,
  IMAGE_SYM_TYPE_VOID = 1,
  IMAGE_SYM_TYPE_CHAR = 2,
  IMAGE_SYM_TYPE_SHORT = 3,
  IMAGE_SYM_TYPE_INT = 4,
  IMAGE_SYM_TYPE_LONG = 5,
  IMAGE_SYM_TYPE_FLOAT = 6,
  IMAGE_SYM_TYPE_DOUBLE = 7,
  IMAGE_SYM_TYPE_STRUCT = 8,
  IMAGE_SYM_TYPE_UNION = 9,
  IMAGE_SYM_TYPE_ENUM = 10,
  IMAGE_SYM_TYPE_MOE = 11,
  IMAGE_SYM_TYPE_BYTE = 12,
  IMAGE_SYM_TYPE_WORD = 13,
  IMAGE_SYM_TYPE_UINT = 14,
  IMAGE_SYM_TYPE_DWORD = 15,
  /* MSB */
  IMAGE_SYM_DTYPE_NULL = 0,
  IMAGE_SYM_DTYPE_POINTER = 1,
  IMAGE_SYM_DTYPE_FUNCTION = 2,
  IMAGE_SYM_DTYPE_ARRAY = 3,
};

/* 4.4.4. Storage Class */
enum
{
  IMAGE_SYM_CLASS_END_OF_FUNCTION = -1,
  IMAGE_SYM_CLASS_NULL = 0,
  IMAGE_SYM_CLASS_AUTOMATIC = 1,
  IMAGE_SYM_CLASS_EXTERNAL = 2,
  IMAGE_SYM_CLASS_STATIC = 3,
  IMAGE_SYM_CLASS_REGISTER = 4,
  IMAGE_SYM_CLASS_EXTERNAL_DEF = 5,
  IMAGE_SYM_CLASS_LABEL = 6,
  IMAGE_SYM_CLASS_UNDEFINED_LABEL = 7,
  IMAGE_SYM_CLASS_MEMBER_OF_STRUCT = 8,
  IMAGE_SYM_CLASS_ARGUMENT = 9,
  IMAGE_SYM_CLASS_STRUCT_TAG = 10,
  IMAGE_SYM_CLASS_MEMBER_OF_UNION = 11,
  IMAGE_SYM_CLASS_UNION_TAG = 12,
  IMAGE_SYM_CLASS_TYPE_DEFINITION = 13,
  IMAGE_SYM_CLASS_UNDEFINED_STATIC = 14,
  IMAGE_SYM_CLASS_ENUM_TAG = 15,
  IMAGE_SYM_CLASS_MEMBER_OF_ENUM = 16,
  IMAGE_SYM_CLASS_REGISTER_PARAM = 17,
  IMAGE_SYM_CLASS_BIT_FIELD = 18,
  IMAGE_SYM_CLASS_BLOCK = 100,
  IMAGE_SYM_CLASS_FUNCTION = 101,
  IMAGE_SYM_CLASS_END_OF_STRUCT = 102,
  IMAGE_SYM_CLASS_FILE = 103,
  IMAGE_SYM_CLASS_SECTION = 104,
  IMAGE_SYM_CLASS_WEAK_EXTERNAL = 105,
  IMAGE_SYM_CLASS_CLR_TOKEN = 107,
};

#endif /* COFF_H */
