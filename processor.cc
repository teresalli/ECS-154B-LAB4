
#include <cstring>
#include <iostream>

#include "memory.hh"
#include "processor.hh"
#include "ticked_object.hh"
#include "util.hh"

Processor::Processor() : cache(nullptr), memory(nullptr), blocked(false),
    totalRequests(0)
{
    createRecords();
    Record &r = trace.front();
    schedule(r.ticksFromNow, [this, &r]{sendRequest(r);});
}

Processor::~Processor()
{
    std::cout << "Total requests: " << totalRequests << std::endl;
}

void
Processor::sendRequest(Record &r)
{
    if (r.write) {
        r.data = new uint8_t[r.size];
        memset(r.data, (uint8_t)r.requestId, r.size);
    }

    DPRINT("Sending request 0x" << std::hex << r.address
            << std::dec << ":" << r.size << " (" << r.requestId << ")");
    outstanding[r.requestId] = r;
    if (cache->receiveRequest(r.address, r.size, r.data, r.requestId)) {
        totalRequests++;
        trace.pop();

        if (trace.empty()) return;

        // Queue the next request.
        Record &next = trace.front();
        schedule(r.ticksFromNow, [this, &next]{sendRequest(next);});
    } else {
        DPRINT("Cache is blocked. Wait for later.");
        // Cache is blocked wait for later.
        blocked = true;
        // Remove the last thing we added to the outstanding list, it's not
        // outstanding.
        outstanding.erase(r.requestId);
    }
}

void
Processor::receiveResponse(int request_id, const uint8_t* data)
{
    // Check to make sure the data is correct!
    DPRINT("Got response for id " << request_id);

    auto it = outstanding.find(request_id);
    assert(it != outstanding.end());
    checkData(it->second, data);
    if (it->second.write) {
        delete[] it->second.data;
    }
    outstanding.erase(it);

    if (blocked) {
        // unblock now.
        DPRINT("Unblocking processor at " << curTick());
        blocked = false;
        Record &r = trace.front();
        schedule(r.ticksFromNow, [this, &r]{sendRequest(r);});
    }
}

int
Processor::getAddrSize()
{
    return 32;
}

void
Processor::checkData(Record &record, const uint8_t* cache_data)
{
    assert(memory);
    if (record.write) {
        memory->processorWrite(record.address, record.size, record.data);
    } else {
        memory->checkRead(record.address, record.size, cache_data);
    }
}

void
Processor::createRecords()
{
    //          Ticks   Write Address    ID#    size
/*    trace.push({5,      1,    0x10000,   1,     8});
    trace.push({5,      1,    0x10008,   2,     8});
    trace.push({5,      1,    0x12000,   3,     8});
    trace.push({5,      1,    0x1c000,   4,     8});
    trace.push({5,      1,    0x10a00,   5,     8});
    trace.push({5,      0,    0x10010,   6,     8});
    trace.push({5,      0,    0x10018,   7,     8});
    trace.push({5,      0,    0x10004,   8,     4});
    trace.push({5,      0,    0x10008,   9,     4});
    trace.push({5,      0,    0x1000c,   10,    4});
    trace.push({5,      0,    0x110000,  11,    4});
*/

    trace.push({5,     0,      0x3588e9c   ,    1,   4});
    trace.push({5,     1,      0x2377f8    ,    2,   8});
    trace.push({5,     0,      0x80ed94    ,    3,   1});
    trace.push({5,     0,      0x3dfec9f   ,    4,   1});
    trace.push({5,     1,      0x17652b0   ,    5,   2});
    trace.push({5,     0,      0x3dfec9c   ,    6,   4});
    trace.push({5,     0,      0x80ed94    ,    7,   2});
    trace.push({5,     1,      0x2377fb    ,    8,   1});
    trace.push({5,     0,      0x17652b0   ,    9,   4});
    trace.push({5,     1,      0x3588e9d   ,   10,   1});
    trace.push({5,     0,      0x17652b0   ,   11,   2});
    trace.push({5,     0,      0x3588e9c   ,   12,   2});
    trace.push({5,     0,      0x17652b0   ,   13,   2});
    trace.push({5,     1,      0x17652b0   ,   14,   2});
    trace.push({5,     1,      0x3dfec9c   ,   15,   4});
    trace.push({5,     1,      0x17652b0   ,   16,   8});
    trace.push({5,     1,      0x2377f8    ,   17,   8});
    trace.push({5,     0,      0x3588e9c   ,   18,   2});
    trace.push({5,     1,      0x3dfec9f   ,   19,   1});
    trace.push({5,     1,      0x17652b0   ,   20,   8});

}
