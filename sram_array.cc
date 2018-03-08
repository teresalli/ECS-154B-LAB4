
#include "sram_array.hh"

SRAMArray::SRAMArray(int64_t lines, int line_bytes) :
    lines(lines), lineBytes(line_bytes)
{
    data.resize(lines * lineBytes);

    totalSize += getSize();
}

uint8_t*
SRAMArray::getLine(int index)
{
    return &data.data()[index * lineBytes];
}

int64_t
SRAMArray::getSize()
{
    return lines * lineBytes;
}

int64_t
SRAMArray::getTotalSize()
{
    return totalSize;
}

int64_t SRAMArray::totalSize = 0;
