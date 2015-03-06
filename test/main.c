#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <floload.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

void* flo_load( const char* name, unsigned int* size )
{
  FILE* file = fopen( name, "rb" );
  
  if ( file != NULL )
  {
    fseek( file, 0, SEEK_END );
    *size = ftell( file );
    fseek( file, 0, SEEK_SET );
    
    void* flo = malloc( *size );
    
    if ( flo )
    {
      fread( flo, 1, *size, file );
      fclose( file );
      return flo;
    }
    
    fclose( file );
  }
  
  return NULL;
}

uintptr_t flo_get_symbol( const char* name )
{
  uintptr_t addr = 0;
  
  if ( !strcmp( name, "printf" ) )
  {
    addr = (uintptr_t)printf;
  }
  
  printf( "returning address of symbol %s (0x%08x)\n", name, addr );
  return addr;
}

static uintptr_t say_hello;

int flo_put_symbol( const char* name, uintptr_t address )
{
  printf( "received address of symbol %s (0x%08x)\n", name, address );
  
  if ( !strcmp( name, "SayHello" ) )
  {
    say_hello = address;
  }
  
  return 0;
}

typedef void (*say_hello_func)( void );

int main()
{
  // load .flo into memory
  unsigned size;
  void* test = flo_load( "test.flo", &size );
  
  printf( "test.flo loaded at 0x%08x\n", test );
  
  // relocate
  const char* extra;
  flo_relocate( test, size, &extra );
  
  // enable execute. we shouldn't call VirtualProtect on memory returned by
  // malloc, because the granularity of VP is pages, so the call below could
  // include internal heap structures. since we're just setting the execute
  // bit, all should be well.
  DWORD old_protect;
  
  if ( !VirtualProtect( test, size, PAGE_EXECUTE_READWRITE, &old_protect ) )
  {
    printf( "error setting the execute bit: %u\n", GetLastError() );
    return 1;
  }
  
  // flush the instruction cache, although i think it isn't necessary in this
  // contrived example.
  // this should work but isn't. sigh.
  /*
  if ( !FlushInstructionCache( GetModuleHandle( NULL ), test, size ) )
  {
    printf( "error flushing the instruction cache: %u\n", GetLastError() );
    return 1;
  }
  */
  
  // execute SayHello()
  printf( "running SayHello()\n" );
  printf( "------------------------------\n" );
  say_hello_func sh = (say_hello_func)say_hello;
  sh();
  printf( "------------------------------\n" );
  
  // cleanup
  free( test );
  
  printf( "bye\n" );
  return 0;
}
