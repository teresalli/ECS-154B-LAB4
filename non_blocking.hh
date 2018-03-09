#ifndef CSIM_NON_BLOCKING_H
#define CSIM_NON_BLOCKING_H

#include <queue>
#include "set_assoc.hh"
#include "tag_array.hh"
#include "sram_array.hh"
using namespace std;

class NonBlockingCache: public SetAssociativeCache
{
public:
    /**
     * @size is the *total* size of the cache in bytes
     * @memory is below this cache
     * @processor this cache is connected to
     * @ways the number of ways in this set associative cache. If the number
     *        of ways cannot be realized, this will cause an error
     * @mshrs number of MSHRs (or max number of concurrent outstanding requests)
     */
    NonBlockingCache(int64_t size, Memory& memory, Processor& processor,
                     int ways, int mshrs);
    
    /**
     * Destructor
     */
    ~NonBlockingCache() override;
    
    /**
     * Called when the processors sends load or store request.
     * All requests can be assummed to be naturally aligned (e.g., a 4 byte
     * request will be aligned to a 4 byte boundary)
     *
     * @param address of the request
     * @param size in bytes of the request.
     * @param data is non-null, then this is a store request.
     * @param request_id the id that must be used when replying to this request
     *
     * @return true if the request can be received, false if the cache is
     *         blocked and the request must be retried later.
     */
    bool receiveRequest(uint64_t address, int size, const uint8_t* data,
                        int request_id) override;
    
    /**
     * Called when memory id finished processing a request.
     * Data will always be the length of memory.getLineSize()
     *
     * @param request_id is the id assigned to this request in sendMemRequest
     * @param data is the data from memory (length of data is line length)
     *        NOTE: This pointer will be invalid when this function returns.
     */
    void receiveMemResponse(int request_id, const uint8_t* data) override;
    
private:
    struct Entry {
        uint64_t savedAddr;
        int savedSize;
        int savedId;
        int target;
        const uint8_t* savedData;
    };
    
    struct MSHR {
        uint64_t blockAddr; // line address
        int issued;
        queue<Entry> entry;
    };
    MSHR* mshr;
    int numMshr;
    bool stall;
    void copyDataIntoCache(Entry entry, const uint8_t* data);
    int searchMSHR(uint64_t blockAddr);
    int findFreeMSHR();
    bool fullMSHR();
};

#endif
