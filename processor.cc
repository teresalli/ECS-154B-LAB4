
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
    /*
    trace.push({5,      1,    0x10000,   1,     8});
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
    trace.push({1,     1,      0x343ae85   ,    0,   1});
    trace.push({2,     1,      0x2a66acc   ,    1,   4});
    trace.push({2,     1,      0x112f84b   ,    2,   1});
    trace.push({3,     0,      0x1103bc8   ,    3,   1});
    trace.push({3,     1,      0x3ab3f90   ,    4,   8});
    trace.push({3,     1,      0x1eca42d   ,    5,   1});
    trace.push({0,     0,      0x2bb8720   ,    6,   8});
    trace.push({3,     0,      0x21e5d38   ,    7,   1});
    trace.push({3,     0,      0x10019fb   ,    8,   1});
    trace.push({0,     1,      0x1095488   ,    9,   8});
    trace.push({0,     1,      0x1eca428   ,   10,   8});
    trace.push({2,     0,      0x21e5d38   ,   11,   4});
    trace.push({0,     1,      0x343ae84   ,   12,   4});
    trace.push({2,     0,      0x1eca42c   ,   13,   4});
    trace.push({3,     1,      0x10019f8   ,   14,   4});
    trace.push({2,     1,      0x112f848   ,   15,   8});
    trace.push({1,     0,      0x21e5d38   ,   16,   8});
    trace.push({0,     0,      0x2bb8720   ,   17,   8});
    trace.push({1,     1,      0x112f848   ,   18,   4});
    trace.push({0,     1,      0x10019fb   ,   19,   1});
    trace.push({0,     0,      0x10019fa   ,   20,   2});
    trace.push({0,     1,      0x10019fa   ,   21,   2});
    trace.push({0,     0,      0x10019fa   ,   22,   2});
    trace.push({3,     0,      0x112f848   ,   23,   4});
    trace.push({3,     1,      0x1095488   ,   24,   8});
    trace.push({1,     1,      0x1eca42c   ,   25,   4});
    trace.push({2,     0,      0x2a66ac8   ,   26,   8});
    trace.push({0,     0,      0x21e5d38   ,   27,   4});
    trace.push({1,     1,      0x1eca42c   ,   28,   2});
    trace.push({1,     0,      0x10019fa   ,   29,   2});
    trace.push({2,     0,      0x109548a   ,   30,   1});
    trace.push({1,     1,      0x2bb8726   ,   31,   2});
    trace.push({0,     0,      0x1095488   ,   32,   8});
    trace.push({0,     1,      0x21e5d38   ,   33,   4});
    trace.push({2,     0,      0x1103bc8   ,   34,   4});
    trace.push({1,     0,      0x1103bc8   ,   35,   2});
    trace.push({3,     1,      0x343ae84   ,   36,   4});
    trace.push({2,     1,      0x21e5d38   ,   37,   8});
    trace.push({3,     0,      0x2bb8726   ,   38,   1});
    trace.push({2,     0,      0x10019f8   ,   39,   8});
    trace.push({2,     0,      0x1095488   ,   40,   4});
    trace.push({2,     0,      0x1eca428   ,   41,   8});
    trace.push({3,     0,      0x2a66ac8   ,   42,   8});
    trace.push({3,     1,      0x10019fb   ,   43,   1});
    trace.push({1,     1,      0x3ab3f95   ,   44,   1});
    trace.push({2,     1,      0x2a66ac8   ,   45,   8});
    trace.push({3,     1,      0x21e5d38   ,   46,   1});
    trace.push({3,     0,      0x10019f8   ,   47,   8});
    trace.push({2,     1,      0x2bb8726   ,   48,   1});
    trace.push({3,     1,      0x2a66acc   ,   49,   4});
    trace.push({3,     1,      0x3ab3f90   ,   50,   8});
    trace.push({1,     1,      0x1eca42d   ,   51,   1});
    trace.push({2,     1,      0x343ae84   ,   52,   4});
    trace.push({2,     1,      0x343ae84   ,   53,   2});
    trace.push({0,     1,      0x1103bc8   ,   54,   4});
    trace.push({1,     1,      0x21e5d38   ,   55,   1});
    trace.push({3,     0,      0x343ae84   ,   56,   2});
    trace.push({0,     0,      0x2a66ace   ,   57,   1});
    trace.push({3,     0,      0x21e5d38   ,   58,   8});
    trace.push({1,     0,      0x2bb8726   ,   59,   1});
}
