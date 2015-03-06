// ATTENTION: COMPILE ME WITH -mcmodel=small

#include <stdio.h>

static const char* GetMessage( void )
{
  return "Hello, world!";
}

void SayHello( void )
{
  printf( "%s %p\n", GetMessage(), printf );
}
