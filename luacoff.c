#include <string.h>
#include <stdlib.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include "coff.h"

static int countBits( unsigned int v )
{
  v = v - ( ( v >> 1 ) & 0x55555555U );                          // reuse input as temporary
  v = ( v & 0x33333333U ) + ( ( v >> 2 ) & 0x33333333U );        // temp
  return ( ( v + ( v >> 4 ) & 0xF0F0F0FU ) * 0x1010101U ) >> 24; // count
}

#define UD_RAWDATA "COFFRawData"

typedef struct
{
  unsigned int size;
  uint8_t      data[ 0 ];
}
rawdata_ud;

static rawdata_ud* rawdata_check( lua_State* L, int index )
{
  return (rawdata_ud*)luaL_checkudata( L, index, UD_RAWDATA );
}

static int rawdata_getSize( lua_State* L )
{
  rawdata_ud* ud = rawdata_check( L, 1 );
  lua_pushunsigned( L, ud->size );
  return 1;
}

static int rawdata_getByte( lua_State* L )
{
  rawdata_ud* ud = rawdata_check( L, 1 );
  unsigned int offset = luaL_checkunsigned( L, 2 );
  
  if ( offset < ud->size )
  {
    lua_pushunsigned( L, ud->data[ offset ] );
    return 1;
  }
  
  return luaL_error( L, "Offset %d out of range [0, %d].", offset, ud->size - 1 );
}

static int rawdata_tostring( lua_State* L )
{
  rawdata_ud* ud = rawdata_check( L, 1 );
  lua_pushfstring( L, UD_RAWDATA "@%p", ud );
  return 1;
}

static int rawdata_push( lua_State* L, void* data, unsigned int size )
{
  static const luaL_Reg methods[] =
  {
    { "getSize",    rawdata_getSize },
    { "getByte",    rawdata_getByte },
    { "__tostring", rawdata_tostring },
    { NULL, NULL }
  };
  
  rawdata_ud* ud = (rawdata_ud*)lua_newuserdata( L, sizeof( rawdata_ud ) + size );
  ud->size = size;
  memcpy( ud->data, data, size );
  
  if ( luaL_newmetatable( L, UD_RAWDATA ) != 0 )
  {
    lua_pushvalue( L, -1 );
    lua_setfield( L, -2, "__index" );
    luaL_setfuncs( L, methods, 0 );
  }
  
  lua_setmetatable( L, -2 );
  return 1;
}

#define UD_BUFFER "COFFBuffer"

typedef struct
{
  uint8_t*     data;
  unsigned int size;
  unsigned int reserved;
  unsigned int pageAlignment;
}
buffer_ud;

static int _buffer_reserve( buffer_ud* ud, unsigned int reserved )
{
  if ( reserved <= ud->reserved )
  {
    return 1;
  }
  
  reserved = ( reserved + ud->pageAlignment - 1 ) & ~( ud->pageAlignment - 1 );
  void* data = realloc( ud->data, reserved );
  
  if ( data != NULL )
  {
    ud->data = (uint8_t*)data;
    ud->reserved = reserved;
    
    return 1;
  }
  
  return 0;
}

static int _buffer_grow( buffer_ud* ud, unsigned int ammount )
{
  int ok = _buffer_reserve( ud, ud->size + ammount );
  ud->size += ammount;
  return ok;
}

static buffer_ud* buffer_check( lua_State* L, int index )
{
  return (buffer_ud*)luaL_checkudata( L, index, UD_BUFFER );
}

static int buffer_reserve( lua_State* L )
{
  buffer_ud* ud = buffer_check( L, 1 );
  unsigned int bytes = luaL_checkunsigned( L, 2 );
  
  if ( _buffer_reserve( ud, bytes ) )
  {
    lua_pushvalue( L, 1 );
    return 1;
  }
    
  return luaL_error( L, "Out of memory." );
}

static int buffer_grow( lua_State* L )
{
  buffer_ud* ud = buffer_check( L, 1 );
  unsigned int bytes = luaL_checkunsigned( L, 2 );
  
  if ( _buffer_grow( ud, bytes ) )
  {
    lua_pushvalue( L, 1 );
    return 1;
  }
    
  return luaL_error( L, "Out of memory." );
}

static int buffer_align( lua_State* L )
{
  buffer_ud* ud = buffer_check( L, 1 );
  unsigned int align = luaL_checkunsigned( L, 2 ) - 1;
  unsigned int fill = luaL_optunsigned( L, 3, 0 );
  
  if ( fill <= 255 )
  {
    if ( countBits( ud->pageAlignment ) == 1 )
    {
      unsigned int addr = ud->size;
      unsigned int count = ( ( ud->size + align ) & ~align ) - ud->size;
      
      if ( count != 0 )
      {
        if ( _buffer_grow( ud, count ) )
        {
          memset( ud->data + addr, fill, count );
          
          lua_pushvalue( L, 1 );
          return 1;
        }
      
        return luaL_error( L, "Out of memory." );
      }
      
      lua_pushvalue( L, 1 );
      return 1;
    }
    
    return luaL_error( L, "The buffer page alignment must be a power of 2." );
  }
    
  return luaL_error( L, "Byte %d out of range [0, 255].", fill );
}

static int buffer_getSize( lua_State* L )
{
  buffer_ud* ud = buffer_check( L, 1 );
  lua_pushunsigned( L, ud->size );
  return 1;
}

static int buffer_set8( lua_State* L )
{
  buffer_ud* ud = buffer_check( L, 1 );
  unsigned int addr = luaL_checkunsigned( L, 2 );
  unsigned int u8 = luaL_checkunsigned( L, 3 );
  
  if ( addr < ud->size )
  {
    if ( u8 <= 255 )
    {
      ud->data[ addr ] = u8;
      
      lua_pushvalue( L, 1 );
      return 1;
    }
    
    return luaL_error( L, "Byte %d out of range [0, 255].", u8 );
  }
  
  return luaL_error( L, "Address %d out of range [0, %d].", addr, ud->size - 1 );
}

static int buffer_set16( lua_State* L )
{
  buffer_ud* ud = buffer_check( L, 1 );
  unsigned int addr = luaL_checkunsigned( L, 2 );
  unsigned int u16 = luaL_checkunsigned( L, 3 );
  
  if ( addr < ud->size - 1 )
  {
    if ( u16 <= 65535 )
    {
      ud->data[ addr++ ] = u16 & 255;
      ud->data[ addr ] = u16 >> 8;
      
      lua_pushvalue( L, 1 );
      return 1;
    }
    
    return luaL_error( L, "Word %d out of range [0, 65535].", u16 );
  }
  
  return luaL_error( L, "Address %d out of range [0, %d].", addr, ud->size - 2 );
}

static int buffer_set32( lua_State* L )
{
  buffer_ud* ud = buffer_check( L, 1 );
  unsigned int addr = luaL_checkunsigned( L, 2 );
  unsigned int u32 = luaL_checkunsigned( L, 3 );
  
  if ( addr < ud->size - 3 )
  {
    ud->data[ addr++ ] = u32 & 255;
    ud->data[ addr++ ] = ( u32 >> 8 ) & 255;
    ud->data[ addr++ ] = ( u32 >> 16 ) & 255;
    ud->data[ addr ] = u32 >> 24;
    
    lua_pushvalue( L, 1 );
    return 1;
  }
  
  return luaL_error( L, "Address %d out of range [0, %d].", addr, ud->size - 4 );
}

static int buffer_get32( lua_State* L )
{
  buffer_ud* ud = buffer_check( L, 1 );
  unsigned int addr = luaL_checkunsigned( L, 2 );
  
  if ( addr < ud->size - 3 )
  {
    lua_pushunsigned( L, *(uint32_t*)( ud->data + addr ) );
    return 1;
  }
  
  return luaL_error( L, "Address %d out of range [0, %d].", addr, ud->size - 4 );
}

static int buffer_setString( lua_State* L )
{
  buffer_ud* ud = buffer_check( L, 1 );
  unsigned int addr = luaL_checkunsigned( L, 2 );
  size_t length;
  const char* str = luaL_checklstring( L, 3, &length );
  
  if ( addr < ud->size - length )
  {
    memcpy( ud->data + addr, str, length + 1 );
    
    lua_pushvalue( L, 1 );
    return 1;
  }
  
  return luaL_error( L, "Address %d out of range [0, %d].", addr, ud->size - length - 1 );
}

static int buffer_setRaw( lua_State* L )
{
  buffer_ud* ud = buffer_check( L, 1 );
  unsigned int addr = luaL_checkunsigned( L, 2 );
  rawdata_ud* raw = rawdata_check( L, 3 );
  
  if ( addr < ud->size - ( raw->size + 1 ) )
  {
    memcpy( ud->data + addr, raw->data, raw->size );
    
    lua_pushvalue( L, 1 );
    return 1;
  }
  
  return luaL_error( L, "Address %d out of range [0, %d].", addr, ud->size - raw->size );
}

static int buffer_append8( lua_State* L )
{
  buffer_ud* ud = buffer_check( L, 1 );
  unsigned int u8 = luaL_checkunsigned( L, 2 );
  
  if ( u8 <= 255 )
  {
    if ( _buffer_grow( ud, 1 ) )
    {
      ud->data[ ud->size - 1 ] = u8;
        
      lua_pushvalue( L, 1 );
      return 1;
    }
    
    return luaL_error( L, "Out of memory." );
  }
  
  return luaL_error( L, "Byte %d out of range [0, 255].", u8 );
}

static int buffer_append16( lua_State* L )
{
  buffer_ud* ud = buffer_check( L, 1 );
  unsigned int u16 = luaL_checkunsigned( L, 2 );
  
  if ( u16 <= 65535 )
  {
    if ( _buffer_grow( ud, 2 ) )
    {
      ud->data[ ud->size - 2 ] = u16 & 255;
      ud->data[ ud->size - 1 ] = u16 >> 8;
        
      lua_pushvalue( L, 1 );
      return 1;
    }
      
    return luaL_error( L, "Out of memory." );
  }
  
  return luaL_error( L, "Word %d out of range [0, 65535].", u16 );
}

static int buffer_append32( lua_State* L )
{
  buffer_ud* ud = buffer_check( L, 1 );
  unsigned int u32 = luaL_checkunsigned( L, 2 );
  
  if ( _buffer_grow( ud, 4 ) )
  {
    ud->data[ ud->size - 4 ] = u32 & 255;
    ud->data[ ud->size - 3 ] = ( u32 >> 8 ) & 255;
    ud->data[ ud->size - 2 ] = ( u32 >> 16 ) & 255;
    ud->data[ ud->size - 1 ] = u32 >> 24;
      
    lua_pushvalue( L, 1 );
    return 1;
  }
    
  return luaL_error( L, "Out of memory." );
}

static int buffer_appendString( lua_State* L )
{
  buffer_ud* ud = buffer_check( L, 1 );
  size_t length;
  const char* str = luaL_checklstring( L, 2, &length );
  
  if ( _buffer_grow( ud, length + 1 ) )
  {
    memcpy( ud->data + ud->size - length - 1, str, length + 1 );
      
    lua_pushvalue( L, 1 );
    return 1;
  }
    
  return luaL_error( L, "Out of memory." );
}

static int buffer_appendRaw( lua_State* L )
{
  buffer_ud* ud = buffer_check( L, 1 );
  rawdata_ud* raw = rawdata_check( L, 2 );
  
  if ( _buffer_grow( ud, raw->size ) )
  {
    memcpy( ud->data + ud->size - raw->size, raw->data, raw->size );
    
    lua_pushvalue( L, 1 );
    return 1;
  }
  
  return luaL_error( L, "Out of memory." );
}

static int buffer_get( lua_State* L )
{
  buffer_ud* ud = buffer_check( L, 1 );
  lua_pushlstring( L, (char*)ud->data, ud->size );
  return 1;
}

static int buffer_tostring( lua_State* L )
{
  buffer_ud* ud = buffer_check( L, 1 );
  lua_pushfstring( L, UD_BUFFER "@%p", ud );
  return 1;
}

static int buffer_new( lua_State* L )
{
  static const luaL_Reg methods[] =
  {
    { "reserve",      buffer_reserve },
    { "grow",         buffer_grow },
    { "align",        buffer_align },
    { "getSize",      buffer_getSize },
    { "set8",         buffer_set8 },
    { "set16",        buffer_set16 },
    { "set32",        buffer_set32 },
    { "get32",        buffer_get32 },
    { "setString",    buffer_setString },
    { "setRaw",       buffer_setRaw },
    { "append8",      buffer_append8 },
    { "append16",     buffer_append16 },
    { "append32",     buffer_append32 },
    { "appendString", buffer_appendString },
    { "appendRaw",    buffer_appendRaw },
    { "get",          buffer_get },
    { "__tostring",   buffer_tostring },
    { NULL, NULL }
  };
  
  unsigned int page_alignment = luaL_optunsigned( L, 1, 65536 );
  buffer_ud* ud = (buffer_ud*)lua_newuserdata( L, sizeof( buffer_ud ) );
  ud->data = NULL;
  ud->size = ud->reserved = 0;
  ud->pageAlignment = page_alignment;
  
  if ( countBits( ud->pageAlignment ) != 1 )
  {
    return luaL_error( L, "The buffer page alignment must be a power of 2." );
  }
  
  if ( luaL_newmetatable( L, UD_BUFFER ) != 0 )
  {
    lua_pushvalue( L, -1 );
    lua_setfield( L, -2, "__index" );
    luaL_setfuncs( L, methods, 0 );
  }
  
  lua_setmetatable( L, -2 );
  return 1;
}

#define UD_SYMBOL "COFFSymbol"

typedef struct
{
  unsigned int value;
  int          sectionNumber;
  unsigned int type;
  unsigned int storageClass;
  unsigned int numberOfAuxSymbols;
  char         name[ 9 ];
  const char*  namePtr;
}
symbol_ud;

static symbol_ud* symbol_check( lua_State* L, int index )
{
  return (symbol_ud*)luaL_checkudata( L, index, UD_SYMBOL );
}

static int symbol_getName( lua_State* L )
{
  symbol_ud* ud = symbol_check( L, 1 );
  
  lua_pushstring( L, ud->namePtr );
  return 1;
}

static int symbol_getValue( lua_State* L )
{
  symbol_ud* ud = symbol_check( L, 1 );
  
  lua_pushunsigned( L, ud->value );
  return 1;
}

static int symbol_getSectionNumber( lua_State* L )
{
  symbol_ud* ud = symbol_check( L, 1 );
  
  lua_pushinteger( L, ud->sectionNumber );
  return 1;
}

static int symbol_getType( lua_State* L )
{
  symbol_ud* ud = symbol_check( L, 1 );
  
  lua_pushunsigned( L, ud->type );
  return 1;
}

static int symbol_getStorageClass( lua_State* L )
{
  symbol_ud* ud = symbol_check( L, 1 );
  
  lua_pushunsigned( L, ud->storageClass );
  return 1;
}

static int symbol_getNumberOfAuxSymbols( lua_State* L )
{
  symbol_ud* ud = symbol_check( L, 1 );
  
  lua_pushunsigned( L, ud->numberOfAuxSymbols );
  return 1;
}

static int symbol_getBaseType( lua_State* L )
{
  symbol_ud* ud = symbol_check( L, 1 );
  
  lua_pushunsigned( L, ud->type & 15 );
  return 1;
}

static int symbol_getComplexType( lua_State* L )
{
  symbol_ud* ud = symbol_check( L, 1 );
  
  lua_pushunsigned( L, ud->type >> 4 );
  return 1;
}

static int symbol_tostring( lua_State* L )
{
  symbol_ud* ud = symbol_check( L, 1 );
  lua_pushfstring( L, UD_SYMBOL "@%p", ud );
  return 1;
}

static int symbol_push( lua_State* L, const coff_header_t* header, const  coff_symbol_t* symbol )
{
  static const luaL_Reg methods[] =
  {
    { "getName",               symbol_getName },
    { "getValue",              symbol_getValue },
    { "getSectionNumber",      symbol_getSectionNumber },
    { "getType",               symbol_getType },
    { "getStorageClass",       symbol_getStorageClass },
    { "getNumberOfAuxSymbols", symbol_getNumberOfAuxSymbols },
    { "getBaseType",           symbol_getBaseType },
    { "getComplexType",        symbol_getComplexType },
    { "__tostring",            symbol_tostring },
    { NULL, NULL }
  };
  
  symbol_ud* ud = (symbol_ud*)lua_newuserdata( L, sizeof( symbol_ud ) );
  ud->value = COFF_GET_UINT( *symbol, Value );
  ud->sectionNumber = COFF_GET_INT( *symbol, SectionNumber );
  ud->type = COFF_GET_UINT( *symbol, Type );
  ud->storageClass = COFF_GET_UINT( *symbol, StorageClass );
  ud->numberOfAuxSymbols = COFF_GET_UINT( *symbol, NumberOfAuxSymbols );
  
  if ( COFF_GET_UINT( *symbol, Name.LongName.Zeroes ) != 0 )
  {
    memcpy( ud->name, symbol->Name.ShortName, 8 );
    ud->name[ 8 ] = 0; // Inlined names don't end with a null when they have eight characters.
    ud->namePtr = ud->name;
  }
  else
  {
    ud->namePtr = (const char*)header + COFF_GET_UINT( *header, PointerToSymbolTable ) + COFF_GET_UINT( *header, NumberOfSymbols ) * COFF_SYMBOL_SIZE + COFF_GET_UINT( *symbol, Name.LongName.Offset );
  }
  
  if ( luaL_newmetatable( L, UD_SYMBOL ) != 0 )
  {
    lua_pushvalue( L, -1 );
    lua_setfield( L, -2, "__index" );
    luaL_setfuncs( L, methods, 0 );
  }
  
  lua_setmetatable( L, -2 );
  return 1;
}

#define UD_RELOCATION "COFFRelocation"

typedef struct
{
  unsigned int virtualAddress;
  unsigned int symbolTableIndex;
  unsigned int type;
}
relocation_ud;

static relocation_ud* relocation_check( lua_State* L, int index )
{
  return (relocation_ud*)luaL_checkudata( L, index, UD_RELOCATION );
}

static int relocation_getVirtualAddress( lua_State* L )
{
  relocation_ud* ud = relocation_check( L, 1 );

  lua_pushunsigned( L, ud->virtualAddress );
  return 1;
}

static int relocation_getSymbolTableIndex( lua_State* L )
{
  relocation_ud* ud = relocation_check( L, 1 );

  lua_pushunsigned( L, ud->symbolTableIndex );
  return 1;
}

static int relocation_getType( lua_State* L )
{
  relocation_ud* ud = relocation_check( L, 1 );

  lua_pushunsigned( L, ud->type );
  return 1;
}

static int relocation_tostring( lua_State* L )
{
  relocation_ud* ud = relocation_check( L, 1 );
  lua_pushfstring( L, UD_RELOCATION "@%p", ud );
  return 1;
}

static int relocation_push( lua_State* L, const coff_relocation_t* relocation )
{
  static const luaL_Reg methods[] =
  {
    { "getVirtualAddress",   relocation_getVirtualAddress },
    { "getSymbolTableIndex", relocation_getSymbolTableIndex },
    { "getType",             relocation_getType },
    { "__tostring",          relocation_tostring },
    { NULL, NULL }
  };
  
  relocation_ud* ud = (relocation_ud*)lua_newuserdata( L, sizeof( relocation_ud ) );
  ud->virtualAddress = COFF_GET_UINT( *relocation, VirtualAddress );
  ud->symbolTableIndex = COFF_GET_UINT( *relocation, SymbolTableIndex );
  ud->type = COFF_GET_UINT( *relocation, Type );
  
  if ( luaL_newmetatable( L, UD_RELOCATION ) != 0 )
  {
    lua_pushvalue( L, -1 );
    lua_setfield( L, -2, "__index" );
    luaL_setfuncs( L, methods, 0 );
  }
  
  lua_setmetatable( L, -2 );
  return 1;
}

#define UD_SECTION "COFFSection"

typedef struct
{
  unsigned int virtualSize;
  unsigned int virtualAddress;
  unsigned int sizeOfRawData;
  unsigned int pointerToRawData;
  unsigned int pointerToRelocations;
  unsigned int pointerToLineNumbers;
  unsigned int numberOfRelocations;
  unsigned int numberOfLineNumbers;
  unsigned int characteristics;
  char         name[ 9 ];
  const char*  namePtr;
  int          rawDataRef;
  int          relocationRefs[ 0 ];
}
section_ud;

static section_ud* section_check( lua_State* L, int index )
{
  return (section_ud*)luaL_checkudata( L, index, UD_SECTION );
}

static int section_getName( lua_State* L )
{
  section_ud* ud = section_check( L, 1 );
  lua_pushstring( L, ud->namePtr );
  return 1;
}

static int section_getVirtualSize( lua_State* L )
{
  section_ud* ud = section_check( L, 1 );
  lua_pushunsigned( L, ud->virtualSize );
  return 1;
}

static int section_getVirtualAddress( lua_State* L )
{
  section_ud* ud = section_check( L, 1 );
  lua_pushunsigned( L, ud->virtualAddress );
  return 1;
}

static int section_getSizeOfRawData( lua_State* L )
{
  section_ud* ud = section_check( L, 1 );
  lua_pushunsigned( L, ud->sizeOfRawData );
  return 1;
}

static int section_getNumberOfRelocations( lua_State* L )
{
  section_ud* ud = section_check( L, 1 );
  lua_pushunsigned( L, ud->numberOfRelocations );
  return 1;
}

static int section_getCharacteristics( lua_State* L )
{
  section_ud* ud = section_check( L, 1 );
  lua_pushunsigned( L, ud->characteristics & ~0x00f00000U );
  return 1;
}

static int section_getAlignmentBit( lua_State* L )
{
  section_ud* ud = section_check( L, 1 );
  lua_pushunsigned( L, ( ( ud->characteristics & 0x00f00000U ) >> 20 ) - 1 );
  return 1;
}

static int section_getAlignmentBytes( lua_State* L )
{
  section_ud* ud = section_check( L, 1 );
  lua_pushunsigned( L, 1U << ( ( ( ud->characteristics & 0x00f00000U ) >> 20 ) - 1 ) );
  return 1;
}

static int section_getRawData( lua_State* L )
{
  section_ud* ud = section_check( L, 1 );
  
  if ( ud->rawDataRef != LUA_NOREF )
  {
    lua_rawgeti( L, LUA_REGISTRYINDEX, ud->rawDataRef );
    return 1;
  }
  
  return 0;
}

static int section_getRelocation( lua_State* L )
{
  section_ud* ud = section_check( L, 1 );
  unsigned int index = luaL_checkunsigned( L, 2 );
  
  if ( index < ud->numberOfRelocations )
  {
    lua_rawgeti( L, LUA_REGISTRYINDEX, ud->relocationRefs[ index ] );
    return 1;
  }
  
  return 0;
}

static int section_relocationIterator( lua_State* L )
{
  section_ud* ud = section_check( L, 1 );
  int index = luaL_checkint( L, 2 ) + 1;

  if ( index >= 0 && index < (int)ud->numberOfRelocations )
  {
    lua_pushinteger( L, index );
    lua_rawgeti( L, LUA_REGISTRYINDEX, ud->relocationRefs[ index ] );
    return 2;
  }
  
  return 0;
}

static int section_relocations( lua_State* L )
{
  section_check( L, 1 );

  lua_pushcfunction( L, section_relocationIterator );
  lua_pushvalue( L, 1 );
  lua_pushinteger( L, -1 );
  return 3;
}

 static int section_tostring( lua_State* L )
{
  section_ud* ud = section_check( L, 1 );
  lua_pushfstring( L, UD_SECTION "@%p", ud );
  return 1;
}

static int section_gc( lua_State* L )
{
  section_ud* ud = (section_ud*)lua_touserdata( L, -1 );
  unsigned int i;
  
  luaL_unref( L, LUA_REGISTRYINDEX, ud->rawDataRef );
  
  for ( i = 0; i < ud->numberOfRelocations; i++ )
  {
    luaL_unref( L, LUA_REGISTRYINDEX, ud->relocationRefs[ i ] );
  }
  
  return 0;
}

static int section_push( lua_State* L, const coff_header_t* header, const coff_section_t* section )
{
  static const luaL_Reg methods[] =
  {
    { "getName",                section_getName },
    { "getVirtualSize",         section_getVirtualSize },
    { "getVirtualAddress",      section_getVirtualAddress },
    { "getSizeOfRawData",       section_getSizeOfRawData },
    { "getNumberOfRelocations", section_getNumberOfRelocations },
    { "getCharacteristics",     section_getCharacteristics },
    { "getAlignmentBit",        section_getAlignmentBit },
    { "getAlignmentBytes",      section_getAlignmentBytes },
    { "getRawData",             section_getRawData },
    { "getRelocation",          section_getRelocation },
    { "relocations",            section_relocations },
    { "__tostring",             section_tostring },
    { "__gc",                   section_gc },
    { NULL, NULL }
  };
  
  section_ud* ud = (section_ud*)lua_newuserdata( L, sizeof( section_ud ) + COFF_GET_UINT( *section, NumberOfRelocations ) * sizeof( int ) );
  
  ud->virtualSize = COFF_GET_UINT( *section, VirtualSize );
  ud->virtualAddress = COFF_GET_UINT( *section, VirtualAddress );
  ud->sizeOfRawData = COFF_GET_UINT( *section, SizeOfRawData );
  ud->pointerToRawData = COFF_GET_UINT( *section, PointerToRawData );
  ud->pointerToRelocations = COFF_GET_UINT( *section, PointerToRelocations );
  ud->pointerToLineNumbers = COFF_GET_UINT( *section, PointerToLineNumbers );
  ud->numberOfRelocations = COFF_GET_UINT( *section, NumberOfRelocations );
  ud->numberOfLineNumbers = COFF_GET_UINT( *section, NumberOfLineNumbers );
  ud->characteristics = COFF_GET_UINT( *section, Characteristics );
  
  if ( section->Name[ 0 ] != '/' )
  {
    memcpy( ud->name, section->Name, 8 );
    ud->name[ 8 ] = 0; // Inlined names don't end with a null when they have eight characters.
    ud->namePtr = ud->name;
  }
  else
  {
    ud->namePtr = (const char*)header + COFF_GET_UINT( *header, PointerToSymbolTable ) + COFF_GET_UINT( *header, NumberOfSymbols ) * COFF_SYMBOL_SIZE + atoi( (const char*)section->Name + 1 );
  }
  
  if ( ud->pointerToRawData != 0 )
  {
    rawdata_push( L, (char*)header + ud->pointerToRawData, ud->sizeOfRawData );
    ud->rawDataRef = luaL_ref( L, LUA_REGISTRYINDEX );
  }
  else
  {
    ud->rawDataRef = LUA_NOREF;
  }
  
  const coff_relocation_t* relocation = (const coff_relocation_t*)( (const char*)header + COFF_GET_UINT( *section, PointerToRelocations ) );
  const coff_relocation_t* relocation_end = (const coff_relocation_t*)( (const char*)relocation + COFF_GET_UINT( *section, NumberOfRelocations ) * COFF_RELOCATION_SIZE );
  unsigned int i;
  
  for ( i = 0; i < COFF_GET_UINT( *section, NumberOfRelocations ); i++ )
  {
    relocation_push( L, relocation );
    ud->relocationRefs[ i ] = luaL_ref( L, LUA_REGISTRYINDEX );
    relocation = (const coff_relocation_t*)( (const char*)relocation + COFF_RELOCATION_SIZE );
  }
  
  if ( luaL_newmetatable( L, UD_SECTION ) != 0 )
  {
    lua_pushvalue( L, -1 );
    lua_setfield( L, -2, "__index" );
    luaL_setfuncs( L, methods, 0 );
  }
  
  lua_setmetatable( L, -2 );
  return 1;
}

#define UD_COFF "COFFHeader"

typedef struct
{
  unsigned int machine;
  unsigned int numberOfSections;
  unsigned int timeDateStamp;
  unsigned int pointerToSymbolTable;
  unsigned int numberOfSymbols;
  unsigned int sizeOfOptionalHeader;
  unsigned int characteristics;
  unsigned int numRefs;
  unsigned int symbolRefsStart;
  int          refs[ 0 ];
}
coff_ud;

static coff_ud* coff_check( lua_State* L, int index )
{
  return (coff_ud*)luaL_checkudata( L, index, UD_COFF );
}

static int coff_getMachine( lua_State* L )
{
  coff_ud* ud = coff_check( L, 1 );
  lua_pushunsigned( L, ud->machine );
  return 1;
}

static int coff_getNumberOfSections( lua_State* L )
{
  coff_ud* ud = coff_check( L, 1 );
  lua_pushunsigned( L, ud->numberOfSections );
  return 1;
}

static int coff_getNumberOfSymbols( lua_State* L )
{
  coff_ud* ud = coff_check( L, 1 );
  lua_pushunsigned( L, ud->numberOfSymbols );
  return 1;
}

static int coff_getCharacteristics( lua_State* L )
{
  coff_ud* ud = coff_check( L, 1 );
  lua_pushunsigned( L, ud->characteristics );
  return 1;
}

static int coff_getSection( lua_State* L )
{
  coff_ud* ud = coff_check( L, 1 );
  unsigned int index = luaL_checkunsigned( L, 2 );
  
  if ( index > 0 && index <= ud->numberOfSections )
  {
    lua_rawgeti( L, LUA_REGISTRYINDEX, ud->refs[ index - 1 ] );
    return 1;
  }
  
  return luaL_error( L, "Section index %d out of range [1, %d].", index, ud->numberOfSections );
}

static int coff_sectionIterator( lua_State* L )
{
  coff_ud* ud = coff_check( L, 1 );
  int index = luaL_checkint( L, 2 ) + 1;

  if ( index > 0 && index <= (int)ud->numberOfSections )
  {
    lua_pushinteger( L, index );
    lua_rawgeti( L, LUA_REGISTRYINDEX, ud->refs[ index - 1 ] );
    return 2;
  }

  return 0;
}

static int coff_sections( lua_State* L )
{
  coff_check( L, 1 );

  lua_pushcfunction( L, coff_sectionIterator );
  lua_pushvalue( L, 1 );
  lua_pushinteger( L, 0 );
  return 3;
}

static int coff_getSymbol( lua_State* L )
{
  coff_ud* ud = coff_check( L, 1 );
  unsigned int index = luaL_checkunsigned( L, 2 );
  
  if ( index < ud->numberOfSymbols )
  {
    lua_rawgeti( L, LUA_REGISTRYINDEX, ud->refs[ index + ud->symbolRefsStart ] );
    return 1;
  }
  
  return luaL_error( L, "Section index %d out of range [0, %d].", index, ud->numberOfSymbols - 1 );
}

static int coff_symbolsIterator( lua_State* L )
{
  coff_ud* ud = coff_check( L, 1 );
  int index = luaL_checkint( L, 2 );

  int skip = 1;
  
  if ( index != -1 )
  {
    lua_rawgeti( L, LUA_REGISTRYINDEX, ud->refs[ index + ud->symbolRefsStart ] );
    symbol_ud* symbol = (symbol_ud*)lua_touserdata( L, -1 );
    skip += symbol->numberOfAuxSymbols;
    lua_pop( L, 1 );
  }
  
  index += skip;
  
  if ( index >= 0 && index < (int)ud->numberOfSymbols )
  {
    lua_pushinteger( L, index );
    lua_rawgeti( L, LUA_REGISTRYINDEX, ud->refs[ index + ud->symbolRefsStart ] );
    return 2;
  }
  
  return 0;
}

static int coff_symbols( lua_State* L )
{
  coff_check( L, 1 );

  lua_pushcfunction( L, coff_symbolsIterator );
  lua_pushvalue( L, 1 );
  lua_pushinteger( L, -1 );
  return 3;
}

static int coff_tostring( lua_State* L )
{
  coff_ud* ud = coff_check( L, 1 );
  lua_pushfstring( L, UD_COFF "@%p", ud );
  return 1;
}

static int coff_gc( lua_State* L )
{
  coff_ud* ud = (coff_ud*)lua_touserdata( L, 1 );
  unsigned int i;
  
  for ( i = 0; i < ud->numRefs; i++ )
  {
    luaL_unref( L, LUA_REGISTRYINDEX, ud->refs[ i ] );
  }
  
  return 0;
}

static int coff_new( lua_State* L )
{
  static const luaL_Reg methods[] =
  {
    { "getMachine",          coff_getMachine },
    { "getNumberOfSections", coff_getNumberOfSections },
    { "getNumberOfSymbols",  coff_getNumberOfSymbols },
    { "getCharacteristics",  coff_getCharacteristics },
    { "getSection",          coff_getSection },
    { "sections",            coff_sections },
    { "getSymbol",           coff_getSymbol },
    { "symbols",             coff_symbols },
    { "__tostring",          coff_tostring },
    { "__gc",                coff_gc },
    { NULL, NULL }
  };
  
  const char* bytes = luaL_checkstring( L, 1 );
  const coff_header_t* header = (const coff_header_t*)bytes;
  unsigned int num_refs = COFF_GET_UINT( *header, NumberOfSections ) + COFF_GET_UINT( *header, NumberOfSymbols );
  
  coff_ud* ud = (coff_ud*)lua_newuserdata( L, sizeof( coff_ud ) + num_refs * sizeof( int ) );
  
  ud->machine = COFF_GET_UINT( *header, Machine );
  ud->numberOfSections = COFF_GET_UINT( *header, NumberOfSections );
  ud->timeDateStamp = COFF_GET_UINT( *header, TimeDateStamp );
  ud->pointerToSymbolTable = COFF_GET_UINT( *header, PointerToSymbolTable );
  ud->numberOfSymbols = COFF_GET_UINT( *header, NumberOfSymbols );
  ud->sizeOfOptionalHeader = COFF_GET_UINT( *header, SizeOfOptionalHeader );
  ud->characteristics = COFF_GET_UINT( *header, Characteristics );
  
  ud->numRefs = num_refs;
  ud->symbolRefsStart = COFF_GET_UINT( *header, NumberOfSections );
  
  const coff_section_t* section = (const coff_section_t*)( (const char*)header + COFF_HEADER_SIZE + COFF_GET_UINT( *header, SizeOfOptionalHeader ) );
  const coff_section_t* section_end = (const coff_section_t*)( (const char*)section + COFF_GET_UINT( *header, NumberOfSections ) * COFF_SECTION_SIZE );
  unsigned index = 0;
  
  while ( section < section_end )
  {
    section_push( L, header, section );
    ud->refs[ index++ ] = luaL_ref( L, LUA_REGISTRYINDEX );
    
    section = (const coff_section_t*)( (const char*)section + COFF_SECTION_SIZE );
  }
  
  const coff_symbol_t* symbol = (const coff_symbol_t*)( (const char*)header + COFF_GET_UINT( *header, PointerToSymbolTable ) );
  const coff_symbol_t* symbol_end = (const coff_symbol_t*)( (const char*)symbol + COFF_GET_UINT( *header, NumberOfSymbols ) * COFF_SYMBOL_SIZE );
  index = 0;
  
  while ( symbol < symbol_end )
  {
    symbol_push( L, header, symbol );
    ud->refs[ index++ + ud->symbolRefsStart ] = luaL_ref( L, LUA_REGISTRYINDEX );
    
    symbol = (const coff_symbol_t*)( (const char*)symbol + COFF_SYMBOL_SIZE );
  }
  
  if ( luaL_newmetatable( L, UD_COFF ) != 0 )
  {
    lua_pushvalue( L, -1 );
    lua_setfield( L, -2, "__index" );
    luaL_setfuncs( L, methods, 0 );
  }
  
  lua_setmetatable( L, -2 );
  
  return 1;
}

int luaopen_coff( lua_State* L )
{
  static const luaL_Reg statics[] =
  {
    { "newCoff", coff_new },
    { "newBuffer", buffer_new },
    { NULL, NULL }
  };

  luaL_newlib( L, statics );
  
  lua_createtable( L, 0, 22 );
  lua_pushunsigned( L, 0x0000U ); lua_setfield( L, -2, "MACHINE_UNKNOWN" );
  lua_pushunsigned( L, 0x01d3U ); lua_setfield( L, -2, "MACHINE_AM33" );
  lua_pushunsigned( L, 0x8664U ); lua_setfield( L, -2, "MACHINE_AMD64" );
  lua_pushunsigned( L, 0x01c0U ); lua_setfield( L, -2, "MACHINE_ARM" );
  lua_pushunsigned( L, 0x01c4U ); lua_setfield( L, -2, "MACHINE_ARMNT" );
  lua_pushunsigned( L, 0xaa64U ); lua_setfield( L, -2, "MACHINE_ARM64" );
  lua_pushunsigned( L, 0x0ebcU ); lua_setfield( L, -2, "MACHINE_EBC" );
  lua_pushunsigned( L, 0x014cU ); lua_setfield( L, -2, "MACHINE_I386" );
  lua_pushunsigned( L, 0x0200U ); lua_setfield( L, -2, "MACHINE_IA64" );
  lua_pushunsigned( L, 0x9041U ); lua_setfield( L, -2, "MACHINE_M32R" );
  lua_pushunsigned( L, 0x0266U ); lua_setfield( L, -2, "MACHINE_MIPS16" );
  lua_pushunsigned( L, 0x0366U ); lua_setfield( L, -2, "MACHINE_MIPSFPU" );
  lua_pushunsigned( L, 0x0466U ); lua_setfield( L, -2, "MACHINE_MIPSFPU16" );
  lua_pushunsigned( L, 0x01f0U ); lua_setfield( L, -2, "MACHINE_POWERPC" );
  lua_pushunsigned( L, 0x01f1U ); lua_setfield( L, -2, "MACHINE_POWERPCFP" );
  lua_pushunsigned( L, 0x0166U ); lua_setfield( L, -2, "MACHINE_R4000" );
  lua_pushunsigned( L, 0x01a2U ); lua_setfield( L, -2, "MACHINE_SH3" );
  lua_pushunsigned( L, 0x01a3U ); lua_setfield( L, -2, "MACHINE_SH3DSP" );
  lua_pushunsigned( L, 0x01a6U ); lua_setfield( L, -2, "MACHINE_SH4" );
  lua_pushunsigned( L, 0x01a8U ); lua_setfield( L, -2, "MACHINE_SH5" );
  lua_pushunsigned( L, 0x01c2U ); lua_setfield( L, -2, "MACHINE_THUMB" );
  lua_pushunsigned( L, 0x0169U ); lua_setfield( L, -2, "MACHINE_WCEMIPSV2" );
  lua_setfield( L, -2, "machines" );
  
  lua_createtable( L, 0, 15 );
  lua_pushunsigned( L, 0x0001U ); lua_setfield( L, -2, "RELOCS_STRIPPED" );
  lua_pushunsigned( L, 0x0002U ); lua_setfield( L, -2, "EXECUTABLE_IMAGE" );
  lua_pushunsigned( L, 0x0004U ); lua_setfield( L, -2, "LINE_NUMS_STRIPPED" );
  lua_pushunsigned( L, 0x0008U ); lua_setfield( L, -2, "LOCAL_SYMS_STRIPPED" );
  lua_pushunsigned( L, 0x0010U ); lua_setfield( L, -2, "AGGRESSIVE_WS_TRIM" );
  lua_pushunsigned( L, 0x0020U ); lua_setfield( L, -2, "LARGE_ADDRESS_AWARE" );
  lua_pushunsigned( L, 0x0080U ); lua_setfield( L, -2, "BYTES_REVERSED_LO" );
  lua_pushunsigned( L, 0x0100U ); lua_setfield( L, -2, "_32BIT_MACHINE" );
  lua_pushunsigned( L, 0x0200U ); lua_setfield( L, -2, "DEBUG_STRIPPED" );
  lua_pushunsigned( L, 0x0400U ); lua_setfield( L, -2, "REMOVABLE_RUN_FROM_SWAP" );
  lua_pushunsigned( L, 0x0800U ); lua_setfield( L, -2, "NET_RUN_FROM_SWAP" );
  lua_pushunsigned( L, 0x1000U ); lua_setfield( L, -2, "SYSTEM" );
  lua_pushunsigned( L, 0x2000U ); lua_setfield( L, -2, "DLL" );
  lua_pushunsigned( L, 0x4000U ); lua_setfield( L, -2, "UP_SYSTEM_ONLY" );
  lua_pushunsigned( L, 0x8000U ); lua_setfield( L, -2, "BYTES_REVERSED_HI" );
  lua_setfield( L, -2, "fileCharacteristics" );
  
  lua_createtable( L, 0, 35 );
  lua_pushunsigned( L, 0x00000008U ); lua_setfield( L, -2, "TYPE_NO_PAD" );
  lua_pushunsigned( L, 0x00000020U ); lua_setfield( L, -2, "CNT_CODE" );
  lua_pushunsigned( L, 0x00000040U ); lua_setfield( L, -2, "CNT_INITIALIZED_DATA" );
  lua_pushunsigned( L, 0x00000080U ); lua_setfield( L, -2, "CNT_UNINITIALIZED_DATA" );
  lua_pushunsigned( L, 0x00000100U ); lua_setfield( L, -2, "LNK_OTHER" );
  lua_pushunsigned( L, 0x00000200U ); lua_setfield( L, -2, "LNK_INFO" );
  lua_pushunsigned( L, 0x00000800U ); lua_setfield( L, -2, "LNK_REMOVE" );
  lua_pushunsigned( L, 0x00001000U ); lua_setfield( L, -2, "LNK_COMDAT" );
  lua_pushunsigned( L, 0x00008000U ); lua_setfield( L, -2, "GPREL" );
  lua_pushunsigned( L, 0x00020000U ); lua_setfield( L, -2, "MEM_PURGEABLE" );
  lua_pushunsigned( L, 0x00020000U ); lua_setfield( L, -2, "MEM_16BIT" );
  lua_pushunsigned( L, 0x00040000U ); lua_setfield( L, -2, "MEM_LOCKED" );
  lua_pushunsigned( L, 0x00080000U ); lua_setfield( L, -2, "MEM_PRELOAD" );
  lua_pushunsigned( L, 0x00100000U ); lua_setfield( L, -2, "ALIGN_1BYTES" );
  lua_pushunsigned( L, 0x00200000U ); lua_setfield( L, -2, "ALIGN_2BYTES" );
  lua_pushunsigned( L, 0x00300000U ); lua_setfield( L, -2, "ALIGN_4BYTES" );
  lua_pushunsigned( L, 0x00400000U ); lua_setfield( L, -2, "ALIGN_8BYTES" );
  lua_pushunsigned( L, 0x00500000U ); lua_setfield( L, -2, "ALIGN_16BYTES" );
  lua_pushunsigned( L, 0x00600000U ); lua_setfield( L, -2, "ALIGN_32BYTES" );
  lua_pushunsigned( L, 0x00700000U ); lua_setfield( L, -2, "ALIGN_64BYTES" );
  lua_pushunsigned( L, 0x00800000U ); lua_setfield( L, -2, "ALIGN_128BYTES" );
  lua_pushunsigned( L, 0x00900000U ); lua_setfield( L, -2, "ALIGN_256BYTES" );
  lua_pushunsigned( L, 0x00a00000U ); lua_setfield( L, -2, "ALIGN_512BYTES" );
  lua_pushunsigned( L, 0x00b00000U ); lua_setfield( L, -2, "ALIGN_1024BYTES" );
  lua_pushunsigned( L, 0x00c00000U ); lua_setfield( L, -2, "ALIGN_2048BYTES" );
  lua_pushunsigned( L, 0x00d00000U ); lua_setfield( L, -2, "ALIGN_4096BYTES" );
  lua_pushunsigned( L, 0x00e00000U ); lua_setfield( L, -2, "ALIGN_8192BYTES" );
  lua_pushunsigned( L, 0x01000000U ); lua_setfield( L, -2, "LNK_NRELOC_OVFL" );
  lua_pushunsigned( L, 0x02000000U ); lua_setfield( L, -2, "MEM_DISCARDABLE" );
  lua_pushunsigned( L, 0x04000000U ); lua_setfield( L, -2, "MEM_NOT_CACHED" );
  lua_pushunsigned( L, 0x08000000U ); lua_setfield( L, -2, "MEM_NOT_PAGED" );
  lua_pushunsigned( L, 0x10000000U ); lua_setfield( L, -2, "MEM_SHARED" );
  lua_pushunsigned( L, 0x20000000U ); lua_setfield( L, -2, "MEM_EXECUTE" );
  lua_pushunsigned( L, 0x40000000U ); lua_setfield( L, -2, "MEM_READ" );
  lua_pushunsigned( L, 0x80000000U ); lua_setfield( L, -2, "MEM_WRITE" );
  lua_setfield( L, -2, "sectionCharacteristics" );
  
  lua_createtable( L, 0, 3 );
  lua_pushinteger( L,  0 ); lua_setfield( L, -2, "UNDEFINED" );
  lua_pushinteger( L, -1 ); lua_setfield( L, -2, "ABSOLUTE" );
  lua_pushinteger( L, -2 ); lua_setfield( L, -2, "DEBUG" );
  lua_setfield( L, -2, "sectionNumbers" );
  
  lua_createtable( L, 0, 20 );
  lua_pushunsigned( L,  0U ); lua_setfield( L, -2, "TYPE_NULL" );
  lua_pushunsigned( L,  1U ); lua_setfield( L, -2, "TYPE_VOID" );
  lua_pushunsigned( L,  2U ); lua_setfield( L, -2, "TYPE_CHAR" );
  lua_pushunsigned( L,  3U ); lua_setfield( L, -2, "TYPE_SHORT" );
  lua_pushunsigned( L,  4U ); lua_setfield( L, -2, "TYPE_INT" );
  lua_pushunsigned( L,  5U ); lua_setfield( L, -2, "TYPE_LONG" );
  lua_pushunsigned( L,  6U ); lua_setfield( L, -2, "TYPE_FLOAT" );
  lua_pushunsigned( L,  7U ); lua_setfield( L, -2, "TYPE_DOUBLE" );
  lua_pushunsigned( L,  8U ); lua_setfield( L, -2, "TYPE_STRUCT" );
  lua_pushunsigned( L,  9U ); lua_setfield( L, -2, "TYPE_UNION" );
  lua_pushunsigned( L, 10U ); lua_setfield( L, -2, "TYPE_ENUM" );
  lua_pushunsigned( L, 11U ); lua_setfield( L, -2, "TYPE_MOE" );
  lua_pushunsigned( L, 12U ); lua_setfield( L, -2, "TYPE_BYTE" );
  lua_pushunsigned( L, 13U ); lua_setfield( L, -2, "TYPE_WORD" );
  lua_pushunsigned( L, 14U ); lua_setfield( L, -2, "TYPE_UINT" );
  lua_pushunsigned( L, 15U ); lua_setfield( L, -2, "TYPE_DWORD" );
  lua_pushunsigned( L,  0U ); lua_setfield( L, -2, "DTYPE_NULL" );
  lua_pushunsigned( L,  1U ); lua_setfield( L, -2, "DTYPE_POINTER" );
  lua_pushunsigned( L,  2U ); lua_setfield( L, -2, "DTYPE_FUNCTION" );
  lua_pushunsigned( L,  3U ); lua_setfield( L, -2, "DTYPE_ARRAY" );
  lua_setfield( L, -2, "symbolTypes" );
  
  lua_createtable( L, 0, 27 );
  lua_pushunsigned( L, 255U ); lua_setfield( L, -2, "END_OF_FUNCTION" );
  lua_pushunsigned( L,   0U ); lua_setfield( L, -2, "NULL" );
  lua_pushunsigned( L,   1U ); lua_setfield( L, -2, "AUTOMATIC" );
  lua_pushunsigned( L,   2U ); lua_setfield( L, -2, "EXTERNAL" );
  lua_pushunsigned( L,   3U ); lua_setfield( L, -2, "STATIC" );
  lua_pushunsigned( L,   4U ); lua_setfield( L, -2, "REGISTER" );
  lua_pushunsigned( L,   5U ); lua_setfield( L, -2, "EXTERNAL_DEF" );
  lua_pushunsigned( L,   6U ); lua_setfield( L, -2, "LABEL" );
  lua_pushunsigned( L,   7U ); lua_setfield( L, -2, "UNDEFINED_LABEL" );
  lua_pushunsigned( L,   8U ); lua_setfield( L, -2, "MEMBER_OF_STRUCT" );
  lua_pushunsigned( L,   9U ); lua_setfield( L, -2, "ARGUMENT" );
  lua_pushunsigned( L,  10U ); lua_setfield( L, -2, "STRUCT_TAG" );
  lua_pushunsigned( L,  11U ); lua_setfield( L, -2, "MEMBER_OF_UNION" );
  lua_pushunsigned( L,  12U ); lua_setfield( L, -2, "UNION_TAG" );
  lua_pushunsigned( L,  13U ); lua_setfield( L, -2, "TYPE_DEFINITION" );
  lua_pushunsigned( L,  14U ); lua_setfield( L, -2, "UNDEFINED_STATIC" );
  lua_pushunsigned( L,  15U ); lua_setfield( L, -2, "ENUM_TAG" );
  lua_pushunsigned( L,  16U ); lua_setfield( L, -2, "MEMBER_OF_ENUM" );
  lua_pushunsigned( L,  17U ); lua_setfield( L, -2, "REGISTER_PARAM" );
  lua_pushunsigned( L,  18U ); lua_setfield( L, -2, "BIT_FIELD" );
  lua_pushunsigned( L, 100U ); lua_setfield( L, -2, "BLOCK" );
  lua_pushunsigned( L, 101U ); lua_setfield( L, -2, "FUNCTION" );
  lua_pushunsigned( L, 102U ); lua_setfield( L, -2, "END_OF_STRUCT" );
  lua_pushunsigned( L, 103U ); lua_setfield( L, -2, "FILE" );
  lua_pushunsigned( L, 104U ); lua_setfield( L, -2, "SECTION" );
  lua_pushunsigned( L, 105U ); lua_setfield( L, -2, "WEAK_EXTERNAL" );
  lua_pushunsigned( L, 107U ); lua_setfield( L, -2, "CLR_TOKEN" );
  lua_setfield( L, -2, "symbolStorageClasses" );
  
  lua_createtable( L, 0, 17 );
  lua_pushunsigned( L, 0x0000U ); lua_setfield( L, -2, "AMD64_ABSOLUTE" );
  lua_pushunsigned( L, 0x0001U ); lua_setfield( L, -2, "AMD64_ADDR64" );
  lua_pushunsigned( L, 0x0002U ); lua_setfield( L, -2, "AMD64_ADDR32" );
  lua_pushunsigned( L, 0x0003U ); lua_setfield( L, -2, "AMD64_ADDR32NB" );
  lua_pushunsigned( L, 0x0004U ); lua_setfield( L, -2, "AMD64_REL32" );
  lua_pushunsigned( L, 0x0005U ); lua_setfield( L, -2, "AMD64_REL32_1" );
  lua_pushunsigned( L, 0x0006U ); lua_setfield( L, -2, "AMD64_REL32_2" );
  lua_pushunsigned( L, 0x0007U ); lua_setfield( L, -2, "AMD64_REL32_3" );
  lua_pushunsigned( L, 0x0008U ); lua_setfield( L, -2, "AMD64_REL32_4" );
  lua_pushunsigned( L, 0x0009U ); lua_setfield( L, -2, "AMD64_REL32_5" );
  lua_pushunsigned( L, 0x000AU ); lua_setfield( L, -2, "AMD64_SECTION" );
  lua_pushunsigned( L, 0x000BU ); lua_setfield( L, -2, "AMD64_SECREL" );
  lua_pushunsigned( L, 0x000CU ); lua_setfield( L, -2, "AMD64_SECREL7" );
  lua_pushunsigned( L, 0x000DU ); lua_setfield( L, -2, "AMD64_TOKEN" );
  lua_pushunsigned( L, 0x000EU ); lua_setfield( L, -2, "AMD64_SREL32" );
  lua_pushunsigned( L, 0x000FU ); lua_setfield( L, -2, "AMD64_PAIR" );
  lua_pushunsigned( L, 0x0010U ); lua_setfield( L, -2, "AMD64_SSPAN32" );
  lua_pushunsigned( L, 0x0000U ); lua_setfield( L, -2, "I386_ABSOLUTE" );
  lua_pushunsigned( L, 0x0001U ); lua_setfield( L, -2, "I386_DIR16" );
  lua_pushunsigned( L, 0x0002U ); lua_setfield( L, -2, "I386_REL16" );
  lua_pushunsigned( L, 0x0006U ); lua_setfield( L, -2, "I386_DIR32" );
  lua_pushunsigned( L, 0x0007U ); lua_setfield( L, -2, "I386_DIR32NB" );
  lua_pushunsigned( L, 0x0009U ); lua_setfield( L, -2, "I386_SEG12" );
  lua_pushunsigned( L, 0x000AU ); lua_setfield( L, -2, "I386_SECTION" );
  lua_pushunsigned( L, 0x000BU ); lua_setfield( L, -2, "I386_SECREL" );
  lua_pushunsigned( L, 0x0014U ); lua_setfield( L, -2, "I386_REL32" );
  lua_setfield( L, -2, "relocationTypes" );
  
  return 1;
}
