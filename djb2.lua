return function( str )
  local hash = 5381
  
  for i = 1, #str do
    local k = str:byte( i )
    hash = bit32.band( hash * 33 + k )
  end
  
  return hash
end
