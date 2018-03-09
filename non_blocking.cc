
#include <cassert>
#include <cstring>
#include "non_blocking.hh"
#include "memory.hh"
#include "processor.hh"
#include "util.hh"

NonBlockingCache::NonBlockingCache(int64_t size, Memory& memory,
                                   Processor& processor, int ways, int mshrs)
: SetAssociativeCache(size, memory, processor, ways),
numMshr(mshrs), stall(false)
{
    assert(mshrs > 0);
    mshr = new MSHR[numMshr];
    for (int i = 0; i < numMshr; i++) {
        mshr[i] = {0, 0};
    }
}

NonBlockingCache::~NonBlockingCache()
{
    delete [] mshr;
}

bool
NonBlockingCache::receiveRequest(uint64_t address, int size,
                                 const uint8_t* data, int request_id)
{
    assert(size <= memory.getLineSize()); // within line size
    // within address range
    assert(address < ((uint64_t)1 << processor.getAddrSize()));
    assert((address & (size - 1)) == 0); // naturally aligned

    if (stall) {
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
        //cout << index << endl;

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
    }
    else
    {
        linenum = findlru(address);
        index = set * way + linenum;
        DPRINT("Miss in cache " << (tagArray.getState(index) & statemask));
        //cout << index << endl;
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
        // need for req id since there are multiple outstanding request.
        // We need to read whether the request is a read or write.
        uint64_t block_address = address & ~(memory.getLineSize() - 1);


        // remember the CPU's request id

        /* deal with mshrs */
        int mshrindex = 0;

        mshrindex = searchMSHR(block_address);
        if (mshrindex != numMshr) // found
        {
            Entry newEntry;
            newEntry.savedAddr = address;
            newEntry.savedSize = size;
            newEntry.target = index;
            newEntry.savedId = request_id;
            newEntry.savedData = data;
            mshr[mshrindex].entry.push(newEntry);
            mshr[mshrindex].issued++;
        }
        else // not found
        {
            if (fullMSHR()) // full
                stall = true;
            else
            {
                // only send mem request if not found block address in mshrs
                sendMemRequest(block_address, memory.getLineSize(), nullptr, request_id);
                mshrindex = findFreeMSHR();
                mshr[mshrindex].issued = 1;
                mshr[mshrindex].blockAddr = block_address;
                Entry newEntry;
                newEntry.savedAddr = address;
                newEntry.savedSize = size;
                newEntry.target = index;
                newEntry.savedId = request_id;
                newEntry.savedData = data;
                mshr[mshrindex].entry.push(newEntry);
            }
        }


    }
    return true;
}

void
NonBlockingCache::receiveMemResponse(int request_id, const uint8_t* data)
{
    assert(data);

    for (int i = 0; i < numMshr; i++)
    {
        if (mshr[i].issued) {
            queue<Entry> e = mshr[i].entry;
            Entry entry;
            while (!e.empty())
            {
                entry = e.front();
                e.pop();
                if (entry.savedId == request_id)
                {
                    while (mshr[i].issued)
                    {
                        entry = mshr[i].entry.front();
                        mshr[i].entry.pop();
                        copyDataIntoCache(entry, data);
                        mshr[i].issued--;
                    }
                    break;
                }
            }
        }


    }

    stall = false;
}

void
NonBlockingCache::copyDataIntoCache(Entry entry, const uint8_t* data)
{
    if (dirty(entry.savedAddr, entry.target - getSetIndex(entry.savedAddr) * way))
    {
        DPRINT("Dirty, writing back");
        // If the line is dirty, then we need to evict it.
        uint8_t* line = dataArray.getLine(entry.target);
        // Calculate the address of the writeback.
        uint64_t wb_address =
        tagArray.getTag(entry.target) << (processor.getAddrSize() - tagBits);
        wb_address |= (getSetIndex(entry.savedAddr) << memory.getLineBits());
        // No response for writes, no need for valid request_id
        sendMemRequest(wb_address, memory.getLineSize(), line, -1);
    }

    int lru = tagArray.getState(entry.target) >> 2;
    int state = (lru << 2) | Invalid;
    tagArray.setState(entry.target, state);
    //setlru(address, linenum);

    // Copy the data into the cache.
    uint8_t* line = dataArray.getLine(entry.target);
    memcpy(line, data, memory.getLineSize());

    assert((tagArray.getState(entry.target) & statemask) == Invalid);

    // Mark valid
    lru = tagArray.getState(entry.target) >> 2;
    state = (lru << 2) | Valid;
    tagArray.setState(entry.target, state);

    // Set tag
    tagArray.setTag(entry.target, getTag(entry.savedAddr));

    // Treat as a hit
    int block_offset = getBlockOffset(entry.savedAddr);

    if (entry.savedData) {
        // if this is a write, copy the data into the cache.
        memcpy(&line[block_offset], entry.savedData, entry.savedSize);
        sendResponse(entry.savedId, nullptr);
        // Mark dirty
        lru = tagArray.getState(entry.target) >> 2;
        state = (lru << 2) | Dirty;
        tagArray.setState(entry.target, state);
    } else {
        // This is a read so we need to return data
        sendResponse(entry.savedId, &line[block_offset]);
    }

}

int
NonBlockingCache::findFreeMSHR()
{
    int index;
    for (index = 0; index < numMshr; index++)
    {
        if (!mshr[index].issued) {
            break;
        }
    }
    return index;
}

int
NonBlockingCache::searchMSHR(uint64_t blockAddr)
{
    int index;
    for (index = 0; index < numMshr; index++)
    {
        if (mshr[index].issued && mshr[index].blockAddr == blockAddr) {
            break;
        }
    }
    return index;
}

bool
NonBlockingCache::fullMSHR()
{
    int index;
    for (index = 0; index < numMshr && mshr[index].issued; index++);

    if (index == numMshr)
        return true;

    return false;
}
