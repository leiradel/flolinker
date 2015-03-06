local FLO_UNUSED   =  0
local FLO_EXPORTED =  1
local FLO_ADDR64   = 16

-- Command line arguments
local inputFiles = {}
local outputFile
local exportFile
local exportSymbol
local verbose = false
local hashfunc

-- List of objects in the order they appear on the command line
local objectList
-- Object (ud) => file name (string)
local objectMap
-- Object (ud) => list of its sections
local objectSections
-- Section (ud) => section name + section number + file name (string)
local sectionNameMap
-- Name (string) => symbol (ud)
local exportMap
-- The machine (from coff.machines)
local machine
-- Object (ud) => object (ud)
local parentMap
-- Name (string) => symbol (ud)
local knownSymbolMap
-- Name (string) => list of relocations (table)
local unknownSymbolMap
-- List of section in the .flo
local sectionList
-- Section (ud) or symbol (name) => offset (number)
local offsetMap
-- The offset of the .bss section
local bssoffset
-- The size of the .bss section
local bsssize
-- The .flo buffer
local flo
-- Trampolines for far calls to undefined symbols
local trampolines
-- Fixups
local fixups
-- Symbol name (string) => offset
local strtable

local function sectionIsAllowed( section )
  local name = section:getName()
  return
    not name:find( '$', 1, true ) and (
      name:sub( 1, 5 ) == '.text' or
      name:sub( 1, 5 ) == '.data' or
      name:sub( 1, 6 ) == '.rdata' or
      name:sub( 1, 4 ) == '.bss'
    )
end

local function info( ... )
    if verbose then
    local args = { ... }
    local format = args[ 1 ]
    table.remove( args, 1 )
    
    for i = 1, #args do
      args[ i ] = tostring( args[ i ] )
    end
    
    io.write( string.format( format, unpack( args ) ):gsub( '\t', '  ' ), '\n' )
  end
end

--                               _                                         _       
--  _ __   __ _ _ __ ___  ___   / \   _ __ __ _ _   _ _ __ ___   ___ _ __ | |_ ___ 
-- | '_ \ / _` | '__/ __|/ _ \ / _ \ | '__/ _` | | | | '_ ` _ \ / _ \ '_ \| __/ __|
-- | |_) | (_| | |  \__ \  __// ___ \| | | (_| | |_| | | | | | |  __/ | | | |_\__ \
-- | .__/ \__,_|_|  |___/\___/_/   \_\_|  \__, |\__,_|_| |_| |_|\___|_| |_|\__|___/
-- |_|                                    |___/                                    

local function usage( out )
  out:write[[
flolink [-?]
flolink [-v] [-e exportfile ] [-s exportsymbol] [-h hashfile]
        -o outputfile inputfile...

-? Help page
-v Be verbose
-e Read list of symbols to export from file (one per line)
-s Symbol to export
-h Use hash function in file instead of strings
-o Output file
]]
end

local function parseArguments( args )
  inputFileList = {}
  
  if #args == 0 then
    usage( io.stderr )
    return -1
  end
  
  local i = 1
  
  while i <= #args do
    if args[ i ] == '-o' then
      if ( i + 1 ) > #args then
        io.stderr:write( 'Error: Missing argumento to -o\n' )
        return -1
      end
      
      i = i + 1
      outputFile = args[ i ]
    elseif args[ i ] == '-e' then
      if ( i + 1 ) > #args then
        io.stderr:write( 'Error: Missing argumento to -e\n' )
        return -1
      end
      
      i = i + 1
      exportFile = args[ i ]
    elseif args[ i ] == '-s' then
      if ( i + 1 ) > #args then
        io.stderr:write( 'Error: Missing argumento to -s\n' )
        return -1
      end
      
      i = i + 1
      exportSymbol = args[ i ]
    elseif args[ i ] == '-v' then
      verbose = true
    elseif args[ i ] == '-h' then
      if ( i + 1 ) > #args then
        io.stderr:write( 'Error: Missing argumento to -h\n' )
        return -1
      end
      
      i = i + 1
      hashfunc = args[ i ]
    elseif args[ i ] == '-?' then
      usage( io.stdout )
      return 0
    else
      inputFileList[ #inputFileList + 1 ] = args[ i ]
    end
    
    i = i + 1
  end
  
  -- Check for mandatory arguments
  if outputFile == nil then
    io.stderr:write( 'Error: Output file not informed\n' )
    return -1
  end
  
  if #inputFileList == 0 then
    io.stderr:write( 'Error: Missing input file(s)\n' )
    return -1
  end
  
  -- Load hash function
  if hashfunc then
    info( 'Loading hash function' )
    
    do
      local file, err = io.open( hashfunc, 'rb' )
      
      if not file then
        io.stderr:write( 'Error: ', err, '\n' )
        return -1
      end
      
      hashfunc = hash.load( file:read( '*a' ) )
      file:close()
    end
  end
end

--  _                 _  ___  _     _           _       
-- | | ___   __ _  __| |/ _ \| |__ (_) ___  ___| |_ ___ 
-- | |/ _ \ / _` |/ _` | | | | '_ \| |/ _ \/ __| __/ __|
-- | | (_) | (_| | (_| | |_| | |_) | |  __/ (__| |_\__ \
-- |_|\___/ \__,_|\__,_|\___/|_.__// |\___|\___|\__|___/
--                               |__/                   

local function loadObjects()
  info( 'Loading objects' )
  objectList = {}
  objectMap = {}
  objectSections = {}
  
  do
    for _, inputFile in ipairs( inputFileList ) do
      info( '\t%s', inputFile )
      local file, err = io.open( inputFile, 'rb' )
      
      if not file then
        io.stderr:write( 'Error: ', err, '\n' )
        return -1
      end
      
      local object = coff.newCoff( file:read( '*a' ) )
      file:close()
      
      local proc = object:getMachine()
      
      if proc ~= coff.machines.MACHINE_AMD64 
        -- and proc ~= coff.machines.MACHINE_I386
      then
        for desc, mach in pairs( coff.machines ) do
          if mach == proc then
            io.stderr:write( 'Error: Don\'t know how to handle machine ', desc, '\n' )
            return -1
          end
        end
        
        io.stderr:write( string.format( 'Error: Unknown machine %04x\n', proc ) )
        return -1
      end
      
      if machine and proc ~= machine then
        io.stderr:write( 'Error: Different machines across object files\n' )
        return -1
      end
      
      machine = proc
      
      objectList[ #objectList + 1 ] = object
      objectMap[ object ] = inputFile
      
      local sectionList = {}
      
      for _, section in object:sections() do
        if sectionIsAllowed( section ) then
          sectionList[ #sectionList + 1 ] = section
        end
      end
      
      objectSections[ object ] = sectionList
    end
  end
end

--  _           _ _     _ ____                      _   __  __             
-- | |__  _   _(_) | __| |  _ \ __ _ _ __ ___ _ __ | |_|  \/  | __ _ _ __  
-- | '_ \| | | | | |/ _` | |_) / _` | '__/ _ \ '_ \| __| |\/| |/ _` | '_ \ 
-- | |_) | |_| | | | (_| |  __/ (_| | | |  __/ | | | |_| |  | | (_| | |_) |
-- |_.__/ \__,_|_|_|\__,_|_|   \__,_|_|  \___|_| |_|\__|_|  |_|\__,_| .__/ 
--                                                                  |_|    

local function buildParentMap()
  parentMap = {}
  sectionNameMap = {}

  -- Build parentship map
  for _, object in ipairs( objectList ) do
    for _, symbol in object:symbols() do
      parentMap[ symbol ] = object
    end
    
    for _, section in ipairs( objectSections[ object ] ) do
      parentMap[ section ] = object
      
      for _, relocation in section:relocations() do
        parentMap[ relocation ] = section
      end
      
      sectionNameMap[ section ] = string.format( '%s@%s', section:getName(), objectMap[ object ] )
    end
  end
end

--  _     ____                  _           _ _  __                          
-- (_)___/ ___| _   _ _ __ ___ | |__   ___ | | |/ /_ __   _____      ___ __  
-- | / __\___ \| | | | '_ ` _ \| '_ \ / _ \| | ' /| '_ \ / _ \ \ /\ / / '_ \ 
-- | \__ \___) | |_| | | | | | | |_) | (_) | | . \| | | | (_) \ V  V /| | | |
-- |_|___/____/ \__, |_| |_| |_|_.__/ \___/|_|_|\_\_| |_|\___/ \_/\_/ |_| |_|
--              |___/                                                        

local function isSymbolKnown( symbol )
  -- AMD64
  local UNDEFINED = coff.sectionNumbers.UNDEFINED
  local ABSOLUTE = coff.sectionNumbers.ABSOLUTE
  local DEBUG = coff.sectionNumbers.DEBUG
  local EXTERNAL = coff.symbolStorageClasses.EXTERNAL
  local STATIC = coff.symbolStorageClasses.STATIC
  local DTYPE_FUNCTION = coff.symbolTypes.DTYPE_FUNCTION
  
  local knownFuncs = {}
  local unknownFuncs = {}
  
  knownFuncs[ coff.machines.MACHINE_AMD64 ] = function( symbol )
    local sn = symbol:getSectionNumber()
    local sc = symbol:getStorageClass()
    local ct = symbol:getComplexType()
    
    return sn ~= UNDEFINED and sn ~= ABSOLUTE and sn ~= DEBUG and ( sc == EXTERNAL or ( sc == STATIC and ct == DTYPE_FUNCTION ) )
  end
  
  unknownFuncs[ coff.machines.MACHINE_AMD64 ] = function( symbol )
    local sn = symbol:getSectionNumber()
    return sn == UNDEFINED
  end
  
  -- Evaluate
  return knownFuncs[ machine ]( symbol ), unknownFuncs[ machine ]( symbol )
end

--  _           _ _     _ _     _     _    ___   __ ____                  _           _     
-- | |__  _   _(_) | __| | |   (_)___| |_ / _ \ / _/ ___| _   _ _ __ ___ | |__   ___ | |___ 
-- | '_ \| | | | | |/ _` | |   | / __| __| | | | |_\___ \| | | | '_ ` _ \| '_ \ / _ \| / __|
-- | |_) | |_| | | | (_| | |___| \__ \ |_| |_| |  _|___) | |_| | | | | | | |_) | (_) | \__ \
-- |_.__/ \__,_|_|_|\__,_|_____|_|___/\__|\___/|_| |____/ \__, |_| |_| |_|_.__/ \___/|_|___/
--                                                        |___/                             

local function buildListOfSymbols()
  knownSymbolMap = {}
  unknownSymbolMap = {}
  
  do
    info( 'Building list of known symbols' )
  
    for _, object in ipairs( objectList ) do
      for _, symbol in object:symbols() do
        local known, unknown = isSymbolKnown( symbol )
        
        if known then
          local name = symbol:getName()
          
          if not knownSymbolMap[ name ] then
            local sectionIndex = symbol:getSectionNumber()
            local section = parentMap[ symbol ]:getSection( sectionIndex )
            
            if sectionIsAllowed( section ) then
              knownSymbolMap[ name ] = symbol
              info( '\t%s defines symbol %s', sectionNameMap[ section ], name )
            end
          end
        end
      end
    end
    
    info( 'Building list of undefined symbols' )
    
    local messages = {}
  
    for _, object in ipairs( objectList ) do
      for _, section in ipairs( objectSections[ object ] ) do
        for _, relocation in section:relocations() do
          local symbol = object:getSymbol( relocation:getSymbolTableIndex() )
          local name = symbol:getName()
          local known, unknown = isSymbolKnown( symbol )
        
          if unknown and not knownSymbolMap[ name ] then
            local list = unknownSymbolMap[ name ] or {}
            list[ #list + 1 ] = relocation
            unknownSymbolMap[ name ] = list
            
            local msg = string.format( '\t%s needs symbol %s', sectionNameMap[ section ], name )
            
            if not messages[ msg ] then
              messages[ msg ] = true
              info( '%s', msg )
            end
          end
        end
      end
    end
  end
end

--  _           _ _     _ _____                       _   __  __             
-- | |__  _   _(_) | __| | ____|_  ___ __   ___  _ __| |_|  \/  | __ _ _ __  
-- | '_ \| | | | | |/ _` |  _| \ \/ / '_ \ / _ \| '__| __| |\/| |/ _` | '_ \ 
-- | |_) | |_| | | | (_| | |___ >  <| |_) | (_) | |  | |_| |  | | (_| | |_) |
-- |_.__/ \__,_|_|_|\__,_|_____/_/\_\ .__/ \___/|_|   \__|_|  |_|\__,_| .__/ 
--                                  |_|                               |_|    

local function buildExportMap()
  exportMap = {}
  local exps = {}
  
  for _, object in ipairs( objectList ) do
    for _, symbol in object:symbols() do
      if symbol:getStorageClass() == coff.symbolStorageClasses.EXTERNAL then
        local known, unknown = isSymbolKnown( symbol )
        
        if known then
          local section = object:getSection( symbol:getSectionNumber() )
          
          if sectionIsAllowed( section ) then
            exps[ symbol:getName() ] = symbol
          end
        end
      end
    end
  end
  
  if exportSymbol then
    info( 'Exporting only %s', exportSymbol )
    
    if not exps[ exportSymbol ] then
      io.stderr:write( 'Error: Exported symbol ', exportSymbol, ' not found\n' )
      return -1
    end
    
    exportMap[ exportSymbol ] = exps[ exportSymbol ]
  elseif exportFile then
    -- Read exported symbols from file
    info( 'Reading exported symbols from %s', exportFile )
    local missing = {}
    local file, err = io.open( exportFile, 'r' )
    
    if not file then
      io.stderr:write( 'Error: ', err, '\n' )
      return -1
    end
    
    for line in file:lines() do
      local name = line:gsub( '%s*([^%s+])%s*', '%1' )
      
      if name and #name ~= 0 then
        if exps[ name ] then
          info( '\t%s', name )
          exportMap[ name ] = exps[ name ]
        else
          info( '\t%s not found in the object files', name )
          missing[ #missing + 1 ] = name
        end
      end
    end
    
    if #missing ~= 0 then
      for _, name in ipairs( missing ) do
        io.stderr:write( 'Error: Exported symbol ', name, ' not found\n' )
      end
      
      return -1
    end
  else
    -- Export all non static symbols
    info( 'Exporting all public symbols' )
    exportMap = exps
  end
  
  for name in pairs( exportMap ) do
    info( '\t%s', name )
  end
end

--  _           _ _     _ _     _     _    ___   __ ____                  _              _ ____            _   _                 
-- | |__  _   _(_) | __| | |   (_)___| |_ / _ \ / _|  _ \ ___  __ _ _   _(_)_ __ ___  __| / ___|  ___  ___| |_(_) ___  _ __  ___ 
-- | '_ \| | | | | |/ _` | |   | / __| __| | | | |_| |_) / _ \/ _` | | | | | '__/ _ \/ _` \___ \ / _ \/ __| __| |/ _ \| '_ \/ __|
-- | |_) | |_| | | | (_| | |___| \__ \ |_| |_| |  _|  _ <  __/ (_| | |_| | | | |  __/ (_| |___) |  __/ (__| |_| | (_) | | | \__ \
-- |_.__/ \__,_|_|_|\__,_|_____|_|___/\__|\___/|_| |_| \_\___|\__, |\__,_|_|_|  \___|\__,_|____/ \___|\___|\__|_|\___/|_| |_|___/
--                                                               |_|                                                             

local function buildListOfRequiredSections2()
  local function addSections( mandatorySet, verbose )
    local list = {}
    local visitedSet = {}
    
    for section in pairs( mandatorySet ) do
      list[ #list + 1 ] = section
      visitedSet[ section ] = true
    end
    
    local i = 1
    
    while list[ i ] do
      local section = list[ i ]
      local object = parentMap[ section ]
      
      for _, relocation in section:relocations() do
        local symbol = object:getSymbol( relocation:getSymbolTableIndex() )
        local index = symbol:getSectionNumber()
        
        if index >= 1 then
          local section2 = object:getSection( index )
          
          if section2 ~= section and sectionIsAllowed( section2 ) then
            info( '\tSection %s for symbol %s used in section %s', sectionNameMap[ section2 ], symbol:getName(), sectionNameMap[ section ] )
            
            if not visitedSet[ section2 ] then
              list[ #list + 1 ] = section2
              visitedSet[ section2 ] = true
            end
          end
        end
      end
      
      i = i + 1
    end
    
    for _, section in ipairs( list ) do
      mandatorySet[ section ] = true
    end
  end
  
  local order = {
    [ '.text' ] = 1,
    [ '.rodata' ] = 2,
    [ '.data' ] = 3,
    [ '.bss' ] = 5
  }

  local function compareSections( s1, s2 )
    local n1 = s1:getName():match( '(.-)$.*' ) or s1:getName()
    local o1 = order[ n1 ] or 4
    local a1 = s1:getAlignmentBytes()
    
    local n2 = s2:getName():match( '(.-)$.*' ) or s2:getName()
    local o2 = order[ n2 ] or 4
    local a2 = s2:getAlignmentBytes()
    
    if o1 ~= o2 then
      -- put same sections toghether
      return o1 < o2
    elseif a1 ~= a2 then
      -- put sections with bigger alignments first
      return a1 > a2
    elseif n1 ~= n2 then
      -- lexical order
      return n1 < n2
    else
      -- order by size
      return s1:getSizeOfRawData() > s2:getSizeOfRawData()
    end
  end

  -- Check mandatory sections (called parts because they have an offset)
  info( 'Building list of required sections' )
  local mandatorySet = {}
  
  for name, symbol in pairs( exportMap ) do
    local section = parentMap[ symbol ]:getSection( symbol:getSectionNumber() )
    info( '\tSection %s exports symbol %s', sectionNameMap[ section ], name )
    
    if not mandatorySet[ section ] then
      mandatorySet[ section ] = true
    end
  end
  
  -- Add more sections to satisfy dependencies
  addSections( mandatorySet, verbose )
  
  -- Build part list and sort
  sectionList = {}
  
  for section in pairs( mandatorySet ) do
    sectionList[ #sectionList + 1 ] = section
  end
  
  table.sort( sectionList, compareSections )
end

local function buildListOfRequiredSections()
  local function addSections( mandatorySet, verbose )
    local list = {}
    local visitedSet = {}
    local messages = {}
    
    for section in pairs( mandatorySet ) do
      list[ #list + 1 ] = section
      visitedSet[ section ] = true
    end
    
    local i = 1
    
    while list[ i ] do
      local section = list[ i ]
      local object = parentMap[ section ]
      
      for _, relocation in section:relocations() do
        local symbol = object:getSymbol( relocation:getSymbolTableIndex() )
        local index = symbol:getSectionNumber()
        
        if index >= 1 then
          local section2 = object:getSection( index )
          
          if section2 ~= section and sectionIsAllowed( section2 ) then
            local msg = string.format( '\tSection %s for symbol %s used in section %s', sectionNameMap[ section2 ], symbol:getName(), sectionNameMap[ section ] )
            
            if not messages[ msg ] then
              messages[ msg ] = true
              info( '%s', msg )
            end
            
            if not visitedSet[ section2 ] then
              list[ #list + 1 ] = section2
              visitedSet[ section2 ] = true
            end
          end
        end
      end
      
      i = i + 1
    end
    
    for _, section in ipairs( list ) do
      mandatorySet[ section ] = true
    end
  end
  
  local order = {
    [ '.text' ] = 1,
    [ '.rodata' ] = 2,
    [ '.data' ] = 3,
    [ '.bss' ] = 5
  }

  local function compareSections( s1, s2 )
    local n1 = s1:getName():match( '(.-)$.*' ) or s1:getName()
    local o1 = order[ n1 ] or 4
    local a1 = s1:getAlignmentBytes()
    
    local n2 = s2:getName():match( '(.-)$.*' ) or s2:getName()
    local o2 = order[ n2 ] or 4
    local a2 = s2:getAlignmentBytes()
    
    if o1 ~= o2 then
      -- put same sections toghether
      return o1 < o2
    elseif a1 ~= a2 then
      -- put sections with bigger alignments first
      return a1 > a2
    elseif n1 ~= n2 then
      -- lexical order
      return n1 < n2
    else
      -- order by size
      return s1:getSizeOfRawData() > s2:getSizeOfRawData()
    end
  end

  -- Check mandatory sections (called parts because they have an offset)
  info( 'Building list of required sections' )
  local mandatorySet = {}
  
  for name, symbol in pairs( exportMap ) do
    local section = parentMap[ symbol ]:getSection( symbol:getSectionNumber() )
    info( '\tSection %s exports symbol %s', sectionNameMap[ section ], name )
    
    if not mandatorySet[ section ] then
      mandatorySet[ section ] = true
    end
  end
  
  -- Add more sections to satisfy dependencies
  addSections( mandatorySet, verbose )
  
  -- Build part list and sort
  sectionList = {}
  
  for section in pairs( mandatorySet ) do
    sectionList[ #sectionList + 1 ] = section
  end
  
  table.sort( sectionList, compareSections )
end

--  _           _ _     _  ___   __  __          _   __  __             
-- | |__  _   _(_) | __| |/ _ \ / _|/ _|___  ___| |_|  \/  | __ _ _ __  
-- | '_ \| | | | | |/ _` | | | | |_| |_/ __|/ _ \ __| |\/| |/ _` | '_ \ 
-- | |_) | |_| | | | (_| | |_| |  _|  _\__ \  __/ |_| |  | | (_| | |_) |
-- |_.__/ \__,_|_|_|\__,_|\___/|_| |_| |___/\___|\__|_|  |_|\__,_| .__/ 
--                                                               |_|    

local function buildOffsetMap()
  info( 'Evaluating offsets' )
  
  -- Evaluate the offsets
  local offset = 0
  bssoffset = nil
  offsetMap = {}
  
  for _, section in ipairs( sectionList ) do
    local alignment = section:getAlignmentBytes() - 1
    
    offset = bit32.band( offset + alignment, bit32.bnot( alignment ) )
    offsetMap[ section ] = offset
    info( '\tSection %s is at 0x%08x', sectionNameMap[ section ], offset )
    
    if not bssoffset and section:getName():sub( 1, 4 ) == '.bss' then
      bssoffset = offset
    end
    
    offset = offset + section:getSizeOfRawData()
  end
  
  if bssoffset then
    bsssize = offset - bssoffset
    info( '\t.bss is at 0x%08x, size is %u', bssoffset, bsssize )
  else
    bssstart = 0
    bsssize = 0
    info( '\tNo .bss section(s) found' )
  end
  
  for name, symbol in pairs( knownSymbolMap ) do
    local object = parentMap[ symbol ]
    local section = object:getSection( symbol:getSectionNumber() )
    
    if offsetMap[ section ] then
      local offset = offsetMap[ section ] + symbol:getValue()
      offsetMap[ symbol:getName() ] = offset
      info( '\tSymbol %s is at 0x%08x', symbol:getName(), offset )
    end
  end
end

--      _                      ____            _   _                _____     _____ _       
--   __| |_   _ _ __ ___  _ __/ ___|  ___  ___| |_(_) ___  _ __  __|_   _|__ |  ___| | ___  
--  / _` | | | | '_ ` _ \| '_ \___ \ / _ \/ __| __| |/ _ \| '_ \/ __|| |/ _ \| |_  | |/ _ \ 
-- | (_| | |_| | | | | | | |_) |__) |  __/ (__| |_| | (_) | | | \__ \| | (_) |  _| | | (_) |
--  \__,_|\__,_|_| |_| |_| .__/____/ \___|\___|\__|_|\___/|_| |_|___/|_|\___/|_|   |_|\___/ 
--                       |_|                                                                

local function dumpSectionsToFlo()
  info( 'Building %s', outputFile )
  flo = coff.newBuffer()
  
  for _, section in ipairs( sectionList ) do
    local alignment = section:getAlignmentBytes()
    
    -- Align
    flo:align( alignment )
    info( '\tAdded section %s at 0x%08x', sectionNameMap[ section ], flo:getSize() )
    
    -- Next offset
    flo:appendRaw( section:getRawData() )
  end
end

--            _     _ _____                                _ _                 
--   __ _  __| | __| |_   _| __ __ _ _ __ ___  _ __   ___ | (_)_ __   ___  ___ 
--  / _` |/ _` |/ _` | | || '__/ _` | '_ ` _ \| '_ \ / _ \| | | '_ \ / _ \/ __|
-- | (_| | (_| | (_| | | || | | (_| | | | | | | |_) | (_) | | | | | |  __/\__ \
--  \__,_|\__,_|\__,_| |_||_|  \__,_|_| |_| |_| .__/ \___/|_|_|_| |_|\___||___/
--                                            |_|                              

local function addTrampolines()
  -- Trampolines for external functions
  info( 'Adding trampolines for undefined symbols' )
  
  fixups = {}
  
  local funcs = {}
  
  funcs[ coff.machines.MACHINE_AMD64 ] = {}
  funcs[ coff.machines.MACHINE_AMD64 ][ coff.relocationTypes.AMD64_REL32 ] = function( object, section, relocation, symbol )
    -- 32-bit displacement from RIP of next instruction to target
    -- This needs to be turned into a trampoline because of REL32 address limits in 64-bit mode
    local name = symbol:getName()
    local addr = offsetMap[ section ] + relocation:getVirtualAddress()

    if not offsetMap[ name ] then
      flo:align( 4 )
      info( '\tAdding trampoline for %s at 0x%08x', name, flo:getSize() )
    
      offsetMap[ name ] = flo:getSize()
      fixups[ #fixups + 1 ] = { name = name, addr = flo:getSize() + 2, type = FLO_ADDR64 }
      
      flo:append8( 0x48 ) -- mov rax, qword 0
      flo:append8( 0xb8 )
      flo:append32( 0 )
      flo:append32( 0 )
      flo:append8( 0xff ) -- jmp rax
      flo:append8( 0xe0 )
    end
  end
  
  for name, relocationList in pairs( unknownSymbolMap ) do
    for _, relocation in ipairs( relocationList ) do
      local section = parentMap[ relocation ]
      local object = parentMap[ section ]
      local symbol = object:getSymbol( relocation:getSymbolTableIndex() )
      
      local func = funcs[ machine ]
      func = func and func[ relocation:getType() ]
      
      if func then
        func( object, section, relocation, symbol )
      else
        io.stderr:write( 'Error: Invalid relocation type (', string.format( '0x%04x', relocation:getType() ), ') for imported symbol ', symbol:getName(), '\n' )
        return -1
      end
    end
  end
end

--           _                 _       
--  _ __ ___| | ___   ___ __ _| |_ ___ 
-- | '__/ _ \ |/ _ \ / __/ _` | __/ _ \
-- | | |  __/ | (_) | (_| (_| | ||  __/
-- |_|  \___|_|\___/ \___\__,_|\__\___|
--

local function relocate()
  -- Resolve relocations
  info( 'Relocating' )
  
  local funcs = {}
  
  funcs[ coff.machines.MACHINE_AMD64 ] = {}
  funcs[ coff.machines.MACHINE_AMD64 ][ coff.relocationTypes.AMD64_REL32 ] = function( object, section, relocation, symbol )
    -- 32-bit displacement from RIP of next instruction to target
    local addr = offsetMap[ section ] + relocation:getVirtualAddress()
    local target = offsetMap[ symbol:getName() ]
    
    if not target then
      for _, section in object:sections() do
        if section:getName() == symbol:getName() then
          target = offsetMap[ section ]
          break
        end
      end
    end
    
    target = target + flo:get32( addr )
    flo:set32( addr, target - ( addr + 4 ) )
    info( '\tSymbol %s at 0x%08x relocated to 0x%08x', symbol:getName(), addr, target )
  end
  
  for _, section in ipairs( sectionList ) do
    local object = parentMap[ section ]
    
    for _, relocation in section:relocations() do
      local symbol = object:getSymbol( relocation:getSymbolTableIndex() )
      
      local func = funcs[ machine ]
      func = func and func[ relocation:getType() ]
      
      if func then
        func( object, section, relocation, symbol )
      else
        io.stderr:write( string.format( 'Error: Invalid relocation type 0x%04x for symbol %s\n', relocation:getType(), symbol:getName() ) )
        return -1
      end
    end
  end
end

--  _           _ _     _ ____                  _           _ _____     _     _      
-- | |__  _   _(_) | __| / ___| _   _ _ __ ___ | |__   ___ | |_   _|_ _| |__ | | ___ 
-- | '_ \| | | | | |/ _` \___ \| | | | '_ ` _ \| '_ \ / _ \| | | |/ _` | '_ \| |/ _ \
-- | |_) | |_| | | | (_| |___) | |_| | | | | | | |_) | (_) | | | | (_| | |_) | |  __/
-- |_.__/ \__,_|_|_|\__,_|____/ \__, |_| |_| |_|_.__/ \___/|_| |_|\__,_|_.__/|_|\___|
--                              |___/                                                

local function buildSymbolTable()
  info( 'Building symbol table' )
  
  strtable = {}
  
  for name, symbol in pairs( exportMap ) do
    local section = parentMap[ symbol ]:getSection( symbol:getSectionNumber() )
    fixups[ #fixups + 1 ] = { name = name, addr = symbol:getValue() + offsetMap[ section ], type = FLO_EXPORTED }
  end
  
  if not hashfunc then
    for _, fixup in ipairs( fixups ) do
      if not strtable[ fixup.name ] then
        strtable[ fixup.name ] = flo:getSize()
        flo:appendString( fixup.name )
      end
    end
  end
  
  flo:align( 4 )
  
  local relocnames = {
    [ FLO_EXPORTED ] = 'exported',
    [ FLO_ADDR64 ] = 'addr64'
  }
  
  for i = 1, #fixups, 4 do
    for j = 0, 3 do
      local fixup = fixups[ i + j ]
      
      if fixup then
        info( '\tAdding entry for %s (%s at 0x%08x)', fixup.name, relocnames[ fixup.type ], fixup.addr )
        flo:append8( fixup.type )
      else
        flo:append8( FLO_UNUSED )
      end
    end
    
    for j = 0, 3 do
      local fixup = fixups[ i + j ]
      
      if fixup then
        local here = flo:getSize()
        
        if hashfunc then
          -- the hash of the symbol
          flo:append32( hashfunc:call( fixup.name ) )
        else
          -- a negative offset to symbol name
          flo:append32( here - strtable[ fixup.name ] )
        end
        
        -- a negative offset to the symbol address
        flo:append32( here - fixup.addr )
      else
        flo:append32( 0 )
        flo:append32( 0 )
      end
    end
  end
end

--   __ _       _     _     _____ _       
--  / _(_)_ __ (_)___| |__ |  ___| | ___  
-- | |_| | '_ \| / __| '_ \| |_  | |/ _ \ 
-- |  _| | | | | \__ \ | | |  _| | | (_) |
-- |_| |_|_| |_|_|___/_| |_|_|   |_|\___/ 
--

local function finishFlo()
  info( 'Writing the header' )
  
  flo:append32( #fixups )
  flo:append32( bssstart )
  flo:append32( bsssize )
  
  local file, err = io.open( outputFile, 'wb' )
  
  if not file then
    io.stderr:write( 'Error: ', err, '\n' )
    return -1
  end
  
  file:write( flo:get() )
  file:close()
end

--                  _
--  _ __ ___   __ _(_)_ __  
-- | '_ ` _ \ / _` | | '_ \ 
-- | | | | | | (_| | | | | |
-- |_| |_| |_|\__,_|_|_| |_|
--

return function( args )
  return parseArguments( args )
      or loadObjects()
      or buildParentMap()
      or buildListOfSymbols()
      or buildExportMap()
      or buildListOfRequiredSections()
      or buildOffsetMap()
      or dumpSectionsToFlo()
      or addTrampolines()
      or relocate()
      or buildSymbolTable()
      or finishFlo()
      or 0
end
