#ifndef CSIM_SET_ASSOC_H
#define CSIM_SET_ASSOC_H

#include "tag_array.hh"
#include "sram_array.hh"
#include "cache.hh"

class SetAssociativeCache: public Cache
{
  public:
    /**
    * @size  the *total* size of the cache in bytes
    * @memory the memory that is below this cache
    * @processor the processor this cache is connected to
    * @ways the number of ways in this set associative cache. If the number
    *        of ways cannot be realized, this will cause an error
    */
    SetAssociativeCache(int64_t size, Memory& memory, Processor& processor,
                        int ways);

    /**
     * Destructor
     */
    ~SetAssociativeCache() override;

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
    virtual bool receiveRequest(uint64_t address, int size, const uint8_t* data,
                                int request_id) override;

    /**
     * Called when memory id finished processing a request.
     * Data will always be the length of memory.getLineSize()
     *
     * @param request_id is the id assigned to this request in sendMemRequest
     * @param data is the data from memory (length of data is line length)
     *        NOTE: This pointer will be invalid when this function returns.
     */
    virtual void receiveMemResponse(int request_id, const uint8_t* data)
        override;

  private:
    enum State {
      Invalid=0,
      Valid=1,
      Invalid2=2,
      Dirty=3 // Dirty implies valid
    };
  
  protected:
    static const int statemask = 3; // 2 bits mask
    static const int NOTHIT = -99; // indicate not hit
    int64_t getSetIndex(uint64_t address); // get set
    int getBlockOffset(uint64_t address); // get offset
    uint64_t getTag(uint64_t address); // get tag
    int hit(uint64_t address); // return hit index
    bool dirty(uint64_t address, int linenum); // check linenum of set is dirty
    int64_t getlru(uint64_t address, int linenum); // extract lru
    void setlru(uint64_t address, int linenum); // reset lru
    int findlru(uint64_t address); // find lru line index
    int way;
    int64_t tagBits;
    uint64_t indexMask;
    TagArray tagArray;
    SRAMArray dataArray;
    struct MSHR {
      int savedId;
      uint64_t savedAddr;
      int target;
      int savedSize;
      const uint8_t* savedData;
    };
    bool blocked;
    MSHR mshr;

};

#endif
