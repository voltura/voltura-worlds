#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace worlds {
// ZIP's reflected IEEE CRC-32 polynomial. Each table entry performs the same
// eight bit steps as the streaming recurrence; callers retain initial/final XOR.
inline constexpr auto crc32Table=[] {
 std::array<uint32_t,256> table{};
 for(uint32_t byte=0;byte<table.size();++byte){uint32_t value=byte;for(unsigned bit=0;bit<8;++bit)value=(value>>1)^(0xedb88320u&uint32_t(-int(value&1)));table[byte]=value;}
 return table;
}();
inline uint32_t crc32Update(uint32_t value,const uint8_t* bytes,size_t count){
 for(size_t i=0;i<count;++i)value=(value>>8)^crc32Table[(value^bytes[i])&255];
 return value;
}
}
