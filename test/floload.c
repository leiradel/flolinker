#include <string.h>
#include <floload.h>

int flo_relocate( void* flo, unsigned int size, const char** extra )
{
  flo_header_t* header = FLO_GET_HEADER( flo, size );
  flo_symbol_block_t* block = FLO_GET_FIRST_BLOCK( header );
  flo_symbol_block_t* end = FLO_GET_LAST_BLOCK( header );
  
  while ( block <= end )
  {
    int i;
    
    for ( i = 0; i < 4; i++ )
    {
      flo_symbol_t* symbol = block->symbols + i;
      
      switch ( block->types[ i ] )
      {
      case FLO_EXPORTED:
        {
          const char* name = FLO_GET_SYMBOL_NAME( symbol );
          
          if ( !flo_put_symbol( name, (uintptr_t)FLO_GET_SYMBOL_ADDRESS( symbol ) ) )
          {
            *extra = name;
            return FLO_ERROR_DEFINING_SYMBOL;
          }
        }
        break;
      
      case FLO_ADDR64:
        {
          const char* name = FLO_GET_SYMBOL_NAME( symbol );
          uintptr_t address = flo_get_symbol( name );
          
          if ( address == 0 )
          {
            *extra = name;
            return FLO_SYMBOL_NOT_FOUND;
          }
          
          FLO_RELOCATE_ADDR64( symbol, address );
        }
        break;
      }
    }
    
    block = FLO_GET_NEXT_BLOCK( block );
  }
  
  memset( FLO_GET_BSSOFFSET( header ), 0, FLO_GET_BSSSIZE( header ) );
  return FLO_OK;
}
