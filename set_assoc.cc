#include <cstring>
#include <cassert>

#include "set_assoc.hh"
#include "memory.hh"
#include "processor.hh"
#include "util.hh"

SetAssociativeCache::SetAssociativeCache(int64_t size, Memory& memory,
                                         Processor& processor, int ways)
: Cache(size, memory, processor), way(ways),
tagBits(processor.getAddrSize()
        - log2int(size / memory.getLineSize() / ways)
        - memory.getLineBits()),
indexMask(size / memory.getLineSize() / way - 1),
tagArray((int) size / memory.getLineSize(),
         log2int(ways) + 2, // lg(ways) for lru bits and 2 for valid and dirty
         (int) tagBits),
dataArray(size / memory.getLineSize(), memory.getLineSize()),
blocked(false),
mshr({-1, 0, 0, 0, nullptr})
{
    assert(ways > 0);
    assert(log2int(ways) + 2 <= 32);
    assert(ways <= size / memory.getLineSize());
    // initialize lru
    for (int i = 0; i < (int) size / memory.getLineSize(); i += way) {
        for (int j = 0; j < way; j++) {
            int lru = (way - 1 - j) << 2;
            int line = j + i;
            tagArray.setState(line, lru);
        }
    }
}

SetAssociativeCache::~SetAssociativeCache()
{

}

bool
SetAssociativeCache::receiveRequest(uint64_t address, int size,
                                    const uint8_t* data, int request_id)
{
    assert(size <= memory.getLineSize()); // within line size
    // within address range
    assert(address < ((uint64_t)1 << processor.getAddrSize()));
    assert((address & (size - 1)) == 0); // naturally aligned

    if (blocked) {
        DPRINT("Cache is blocked!");
        // Cache is currently blocked, so it cannot receive a new request
        return false;
    }
    int set = (int) getSetIndex(address);
    int linenum = hit(address); // get the hit linenum
    int index= set * way + linenum;
    
    if (linenum != NOTHIT) { // hit
        DPRINT("Hit in cache");
        // get a pointer to the data
        uint8_t* line = dataArray.getLine(index);

        int block_offset = getBlockOffset(address);

        if (data) {
            // if this is a write, copy the data into the cache.
            memcpy(&line[block_offset], data, size);
            sendResponse(request_id, nullptr);
            // Mark dirty
            int lru = tagArray.getState(index) >> 2;
            int state = (lru << 2) | Dirty;
            tagArray.setState(index, state);
        } else {
            // This is a read so we need to return data
            sendResponse(request_id, &line[block_offset]);
        }
        setlru(address, linenum);
    } else {
        linenum = findlru(address);
        index = set * way + linenum;
        DPRINT("Miss in cache " << (tagArray.getState(index) & statemask));
        if (dirty(address, linenum)) {
            DPRINT("Dirty, writing back");
            // If the line is dirty, then we need to evict it.
            uint8_t* line = dataArray.getLine(index);
            // Calculate the address of the writeback.
            uint64_t wb_address =
            tagArray.getTag(index) << (processor.getAddrSize() - tagBits);
            wb_address |= (set << memory.getLineBits());
            // No response for writes, no need for valid request_id
            sendMemRequest(wb_address, memory.getLineSize(), line, -1);
        }

        int lru = tagArray.getState(index) >> 2;
        int state = (lru << 2) | Invalid;
        tagArray.setState(index, state);
        setlru(address, linenum);
        // Forward to memory and block the cache.
        // no need for req id since there is only one outstanding request.
        // We need to read whether the request is a read or write.
        uint64_t block_address = address & ~(memory.getLineSize() - 1);
        sendMemRequest(block_address, memory.getLineSize(), nullptr, 0);
        // remember the CPU's request id
        mshr.savedId = request_id;
        // Remember the address
        mshr.savedAddr = address;
        mshr.target = index;
        // Remember the data if it is a write.
        mshr.savedSize = size;
        mshr.savedData = data;
        // Mark the cache as blocked
        blocked = true;
    }
    return true;
}

void
SetAssociativeCache::receiveMemResponse(int request_id, const uint8_t* data)
{
    assert(request_id == 0);
    assert(data);

    // Copy the data into the cache.
    uint8_t* line = dataArray.getLine(mshr.target);
    memcpy(line, data, memory.getLineSize());

    assert((tagArray.getState(mshr.target) & statemask) == Invalid);

    // Mark valid
    int state = Valid;
    tagArray.setState(mshr.target, state);

    // Set tag
    tagArray.setTag(mshr.target, getTag(mshr.savedAddr));

    // Treat as a hit
    int block_offset = getBlockOffset(mshr.savedAddr);

    if (mshr.savedData) {
        // if this is a write, copy the data into the cache.
        memcpy(&line[block_offset], mshr.savedData, mshr.savedSize);
        sendResponse(mshr.savedId, nullptr);
        // Mark dirty
        state = Dirty;
        tagArray.setState(mshr.target, state);
    } else {
        // This is a read so we need to return data
        sendResponse(mshr.savedId, &line[block_offset]);
    }

    blocked = false;
    mshr.savedId = -1;
    mshr.savedAddr = 0;
    mshr.target = 0;
    mshr.savedSize = 0;
    mshr.savedData = nullptr;
}

void SetAssociativeCache::setlru(uint64_t address, int linenum)
{
    int set = (int) getSetIndex(address);
    uint32_t index, state;
    uint32_t lru;
    for (int i = 0; i < way; i++) {
        if (i != linenum && getlru(address, i) < getlru(address, linenum)){
            lru = (uint32_t) (getlru(address, i) + 1) << 2;
            index = set * way + i;
            state = (tagArray.getState(index) & statemask);
            state |= lru;
            tagArray.setState(index, state);
        }
    }

    index = set * way + linenum;
    state = tagArray.getState(index) & statemask;
    tagArray.setState(index, state);
}

int64_t
SetAssociativeCache::getlru(uint64_t address, int linenum)
{
    int set = (int) getSetIndex(address);
    int index = set * way + linenum;
    return tagArray.getState(index) >> 2;
}

int
SetAssociativeCache::findlru(uint64_t address)
{
    int lruindex = 0;
    for (int i = 1; i < way; i++)
    {
        if (getlru(address, i) > getlru(address, lruindex))
            lruindex = i;
    }
    return lruindex;
}

int64_t
SetAssociativeCache::getSetIndex(uint64_t address)
{
    return (address >> memory.getLineBits()) & indexMask;
}

int
SetAssociativeCache::getBlockOffset(uint64_t address)
{
    return address & (memory.getLineSize() - 1);
}

uint64_t
SetAssociativeCache::getTag(uint64_t address)
{
    return address >> (processor.getAddrSize() - tagBits);
}

int
SetAssociativeCache::hit(uint64_t address)
{
    int set = (int) getSetIndex(address);
    int start = set * way;
    for (int index = 0; index < way; index++)
    {
        State state = (State)(tagArray.getState(start + index) & statemask);
        uint64_t line_tag = tagArray.getTag(start + index);
        if ((state == Valid || state == Dirty) && // dirty implies valid
            line_tag == getTag(address))
            return index;
    }

    return NOTHIT;
}

bool
SetAssociativeCache::dirty(uint64_t address, int linenum)
{
    int set = (int) getSetIndex(address);
    int index = set * way + linenum;
    State state = (State)(tagArray.getState(index) & statemask);
    if (state == Dirty)
        return true;
    else
        return false;
}
