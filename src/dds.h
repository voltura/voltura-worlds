#pragma once
#include <dxgiformat.h>
#include <algorithm>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>
namespace worlds {
struct DdsMip {size_t offset;unsigned rowBytes,rows;};
struct DdsImage {unsigned width,height;DXGI_FORMAT format;std::vector<DdsMip> mips;};
// Deliberately narrow: bundled, complete-mip BC4/BC7 (linear or sRGB) 2D textures only.
inline DdsImage parseDds(std::span<const uint8_t> bytes){
 auto fail=[](){throw std::runtime_error("Invalid bundled DDS texture");};
 if(bytes.size()<148||bytes.size()>128u*1024*1024)fail();
 auto word=[&](size_t i){size_t p=i*4;return uint32_t(bytes[p])|uint32_t(bytes[p+1])<<8|uint32_t(bytes[p+2])<<16|uint32_t(bytes[p+3])<<24;};
 if(word(0)!=0x20534444||word(1)!=124||word(19)!=32||word(20)!=4||word(21)!=0x30315844||word(6)>1||word(28)!=0||word(33)!=3||word(34)!=0||word(35)!=1)fail();
 DdsImage image{word(4),word(3),DXGI_FORMAT(word(32)),{}};
 if(!image.width||!image.height||image.width>8192||image.height>8192)fail();
 if(image.format!=DXGI_FORMAT_BC4_UNORM&&image.format!=DXGI_FORMAT_BC7_UNORM_SRGB&&image.format!=DXGI_FORMAT_BC7_UNORM)fail();
 unsigned expected=1;for(unsigned extent=std::max(image.width,image.height);extent>1;extent/=2)expected++;
 if(word(7)!=expected)fail();
 size_t offset=148;unsigned width=image.width,height=image.height,blockBytes=image.format==DXGI_FORMAT_BC4_UNORM?8:16;
 for(unsigned mip=0;mip<expected;mip++){
  unsigned rowBytes=std::max(1u,(width+3)/4)*blockBytes,rows=std::max(1u,(height+3)/4);
  size_t size=size_t(rowBytes)*rows;if(size>bytes.size()-offset)fail();
  image.mips.push_back({offset,rowBytes,rows});offset+=size;width=std::max(1u,width/2);height=std::max(1u,height/2);
 }
 if(offset!=bytes.size())fail();
 return image;
}
}
