
#include <cassert>

#include "non_blocking.hh"
#include "memory.hh"
#include "processor.hh"
#include "util.hh"

NonBlockingCache::NonBlockingCache(int64_t size, Memory& memory,
                                   Processor& processor, int ways, int mshrs)
: SetAssociativeCache(size, memory, processor, ways),
numMshr(mshrs)
{
  assert(mshrs > 0);
  mshr = new MSHR[numMshr];
  for (int i = 0; i < numMshr; i++) {
    mshr[i] = {-1, 0, 0, 0, nullptr};
  }
}

NonBlockingCache::~NonBlockingCache()
{
  
}

bool
NonBlockingCache::receiveRequest(uint64_t address, int size,
                                 const uint8_t* data, int request_id)
{
  if (blocked)
    return false;
  
  int set = (int) getSetIndex(address);
  int linenum = hit(address); // get the hit linenum
  int index= set * way + linenum;
  
  if (linenum != NOTHIT) // hit
  {
    
    
  }
  else // not hit
  {
    
    
    
    int mshrindex = searchMSHR(address);
    if (mshrindex != WRONG) // if found
    {
      
    }
    else
    {
      if (!fullMSHR()) // not full
      {
        MSHR newEntry;
        newEntry.savedAddr = address;
        newEntry.savedData = data;
        newEntry.savedId = request_id;
        newEntry.savedSize = size;
      }
      else
        blocked = false;
    }
  }
  return true;
}

void
NonBlockingCache::receiveMemResponse(int request_id, const uint8_t* data)
{
  
}

int
NonBlockingCache::searchMSHR(int blockindex)
{
  int index;
  for (index = 0; index < numMshr; index++)
  {
    if (mshr[index].target == blockindex) {
      return index;
    }
  }
  return -1;
}

bool
NonBlockingCache::fullMSHR()
{
  int index;
  for (index = 0; index < numMshr; index++)
  {
    if (mshr[index].savedAddr == 0) {
      return false;
    }
  }
  return true;
}
