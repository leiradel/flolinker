#ifndef FLOLOAD_H
#define FLOLOAD_H

#include <stdint.h>

/* Types of symbols. */
#define FLO_UNUSED    0 /* Unused entry. */
#define FLO_EXPORTED  1 /* Exported symbol. */
#define FLO_ADDR64   16 /* Absolute 64-bit address. */

/* Errors */
#define FLO_OK                     0 /* Yay! */
#define FLO_ERROR_DEFINING_SYMBOL -1 /* flo_put_symbol returned zero. */
#define FLO_SYMBOL_NOT_FOUND      -2 /* flo_get_symbol returned zero. */

/* The .flo header, which is located at the end of the file actually. */
typedef struct
{
  uint32_t numsymbols;
  uint32_t bssoffset;
  uint32_t bsssize;
}
flo_header_t;

/* Symbols inside a symbol block. */
typedef struct
{
  union
  {
    uint32_t name; /* A negative offset to the symbol name. */
    uint32_t hash; /* The hash of the symbol. */
  };
  
  uint32_t address; /* A negative offset to the symbol address. */
}
flo_symbol_t;

typedef struct
{
  uint8_t      types[ 4 ];   /* The type of the i'th symbol. */
  flo_symbol_t symbols[ 4 ]; /* The i'th symbol. */
}
flo_symbol_block_t;

/* Get the header of a .flo given its start address and size. */
#define FLO_GET_HEADER( start, size )  ( (flo_header_t*)( (uint8_t*)( start ) + ( size ) - sizeof( flo_header_t ) ) )

/* Get the number of symbols in the .flo. */
#define FLO_GET_NUMSYMBOLS( header ) ( ( header )->numsymbols )
/* Get the address of the start of the .bss section. */
#define FLO_GET_BSSOFFSET( header )  ( (void*)( (uint8_t*)( header ) - ( ( header )->bssoffset ) ) )
/* Get the size of the .bss section. */
#define FLO_GET_BSSSIZE( header )    ( ( header )->bsssize )

/* Get the first symbol block. */
#define FLO_GET_FIRST_BLOCK( header ) ( (flo_symbol_block_t*)( (uint8_t*)( header ) - ( ( ( header )->numsymbols + 3 ) / 4 ) * sizeof( flo_symbol_block_t ) ) )
/* Get the last symbol block. */
#define FLO_GET_LAST_BLOCK( header )  ( (flo_symbol_block_t*)( (uint8_t*)( header ) - sizeof( flo_symbol_t ) ) )
/* Get the next symbol block. */
#define FLO_GET_NEXT_BLOCK( block )   ( (flo_symbol_block_t*)( (uint8_t*)( block ) + sizeof( flo_symbol_block_t ) ) )

/* Get the symbol name. */
#define FLO_GET_SYMBOL_NAME( symbol )    ( (char*)( (uint8_t*)( symbol ) - ( symbol )->name ) )
/* Get the symbol hash. */
#define FLO_GET_SYMBOL_HASH( symbol )    ( ( symbol ).hash )
/* Get the symbol address. */
#define FLO_GET_SYMBOL_ADDRESS( symbol ) ( (void*)( (uint8_t*)( symbol ) - ( symbol )->address ) )

/* Relocate a ADDR64 symbol. */
#define FLO_RELOCATE_ADDR64( symbol, addr ) do { *(uint64_t*)FLO_GET_SYMBOL_ADDRESS( symbol ) = (uint64_t)(uintptr_t)addr; } while ( 0 )

/* Relocate an in-memory .flo, returns one of the errors above. */
int flo_relocate( void* flo, unsigned int size, const char** extra );

/* User-defined functions. */
void*     flo_load( const char* name, unsigned int* size );      /* Load a module into memory. */
uintptr_t flo_get_symbol( const char* name );                    /* Return the address of a symbol. */
int       flo_put_symbol( const char* name, uintptr_t address ); /* Define a symbol. */

#endif /* FLOLOAD_H */
