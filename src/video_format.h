#pragma once
#include <span>
#include <string>
#include <cstdint>
#include <algorithm>
#include <cstring>
#include <vector>
namespace worlds {
inline std::string videoContainer(std::span<const unsigned char> b){
 if(b.size()>=12&&!memcmp(b.data()+4,"ftyp",4))return !memcmp(b.data()+8,"qt  ",4)?"mov":!memcmp(b.data()+8,"M4V ",4)?"m4v":"mp4";
 if(b.size()>=12&&!memcmp(b.data(),"RIFF",4)&&!memcmp(b.data()+8,"AVI ",4))return "avi";
 constexpr unsigned char asf[]={0x30,0x26,0xb2,0x75,0x8e,0x66,0xcf,0x11,0xa6,0xd9,0,0xaa,0,0x62,0xce,0x6c};
 if(b.size()>=16&&!memcmp(b.data(),asf,16))return "wmv";
 if(b.size()>=5&&b[0]==0x1a&&b[1]==0x45&&b[2]==0xdf&&b[3]==0xa3){
  auto vint=[&](size_t& at,bool keep)->uint64_t{if(at>=b.size()||!b[at])return UINT64_MAX;unsigned n=1;while(n<=8&&!(b[at]&(0x80>>(n-1))))++n;if(n>8||n>b.size()-at)return UINT64_MAX;uint64_t value=b[at++]&(keep?255:(0xff>>n));for(unsigned i=1;i<n;i++)value=(value<<8)|b[at++];return value;};
  size_t pos=4;auto length=vint(pos,false);if(length>b.size()-pos)return {};auto end=pos+size_t(length);
  for(unsigned count=0;pos<end&&count<128;count++){auto id=vint(pos,true),size=vint(pos,false);if(size>end-pos)return {};if(id==0x4282){std::string type(reinterpret_cast<const char*>(b.data()+pos),size_t(size));return type=="webm"?"webm":type=="matroska"?"mkv":"";}pos+=size_t(size);}
 }
 return {};
}
inline unsigned h264SequenceTransfer(std::span<const unsigned char> blob);
inline bool legacyAvcMp4(std::span<const unsigned char> b){
 if(videoContainer(b)!="mp4"&&videoContainer(b)!="m4v")return false;
 auto be=[&](size_t p){return (uint32_t(b[p])<<24)|(uint32_t(b[p+1])<<16)|(uint32_t(b[p+2])<<8)|b[p+3];};
 auto scan=[&](auto&& self,size_t from,size_t end,unsigned depth)->bool{
  if(depth>8)return false;
  for(unsigned boxes=0;from+8<=end&&boxes<100000;boxes++){
   uint64_t size=be(from);size_t header=8;if(size==1){if(end-from<16)return false;size=(uint64_t(be(from+8))<<32)|be(from+12);header=16;}if(size==0)size=end-from;if(size<header||size>end-from)return false;
   auto tag=b.data()+from+4;size_t payload=from+header,stop=from+size_t(size);
   if(!memcmp(tag,"stsd",4)){if(stop-payload<8)return false;size_t p=payload+8;for(unsigned i=0;i<be(payload+4)&&i<64;i++){if(stop-p<8)return false;auto n=be(p);if(n<8||n>stop-p)return false;if(!memcmp(b.data()+p+4,"avc1",4)||!memcmp(b.data()+p+4,"avc3",4)){
     if(n<86)return false;bool config=false;size_t child=p+86;
     while(child+8<=p+n){auto count=be(child);if(count<8||count>p+n-child)return false;
      if(!memcmp(b.data()+child+4,"avcC",4)){config=true;auto transfer=h264SequenceTransfer(b.subspan(child+8,count-8));if(transfer==16||transfer==18)return false;}
      if(!memcmp(b.data()+child+4,"colr",4)&&count>=18&&(!memcmp(b.data()+child+8,"nclx",4)||!memcmp(b.data()+child+8,"nclc",4))){unsigned t=(unsigned(b[child+14])<<8)|b[child+15];if(t==16||t==18)return false;}
      child+=count;
     }return config;
    }p+=n;}}
   if(!memcmp(tag,"moov",4)||!memcmp(tag,"trak",4)||!memcmp(tag,"mdia",4)||!memcmp(tag,"minf",4)||!memcmp(tag,"stbl",4))if(self(self,payload,stop,depth+1))return true;
   from=stop;
  }return false;
 };return scan(scan,0,b.size(),0);
}
// H.264 SPS VUI transfer characteristics. Decode only bounded metadata; never
// infer HDR from bit depth or pixel format. Handles avcC and Annex-B headers.
inline unsigned h264SequenceTransfer(std::span<const unsigned char> blob){
 auto spsTransfer=[](std::span<const unsigned char> nal)->unsigned{
  if(nal.empty()||(nal[0]&31)!=7||nal.size()>65536)return 0;
  std::vector<unsigned char> rbsp;for(size_t i=1;i<nal.size();i++){if(i>=3&&nal[i]==3&&nal[i-1]==0&&nal[i-2]==0)continue;rbsp.push_back(nal[i]);}
  size_t at=0;bool ok=true;
  auto bits=[&](unsigned count){unsigned value=0;if(count>32||at+count>rbsp.size()*8){ok=false;return 0u;}for(unsigned j=0;j<count;j++,at++)value=(value<<1)|((rbsp[at/8]>>(7-at%8))&1);return value;};
  auto ue=[&](){unsigned zeros=0;while(ok&&!bits(1)&&zeros<31)++zeros;if(!ok||zeros>=31){ok=false;return 0u;}return ((1u<<zeros)-1)+bits(zeros);};
  auto se=[&](){unsigned n=ue();return n&1?int((n+1)/2):-int(n/2);};
  unsigned profile=bits(8);bits(8);bits(8);ue();
  if(profile==100||profile==110||profile==122||profile==244||profile==44||profile==83||profile==86||profile==118||profile==128||profile==138||profile==139||profile==134||profile==135){
   unsigned chroma=ue();if(chroma>3)return 0;if(chroma==3)bits(1);ue();ue();bits(1);
   if(bits(1))for(unsigned list=0;list<(chroma==3?12u:8u)&&ok;list++)if(bits(1)){int last=8,next=8;for(unsigned j=0;j<(list<6?16u:64u)&&ok;j++){if(next)next=(last+se()+256)%256;if(next)last=next;}}
  }
  ue();unsigned order=ue();if(order==0)ue();else if(order==1){bits(1);se();se();unsigned cycles=ue();if(cycles>256)return 0;for(unsigned j=0;j<cycles&&ok;j++)se();}else if(order>2)return 0;
  ue();bits(1);ue();ue();if(!bits(1))bits(1);bits(1);if(bits(1)){ue();ue();ue();ue();}
  if(!ok||!bits(1))return 0;
  if(bits(1)){if(bits(8)==255){bits(16);bits(16);}}
  if(bits(1))bits(1);
  if(bits(1)){bits(3);bits(1);if(bits(1)){bits(8);unsigned transfer=bits(8);bits(8);return ok?transfer:0;}}
  return 0;
 };
 if(blob.size()>6&&blob[0]==1){size_t at=6;for(unsigned i=0;i<unsigned(blob[5]&31);i++){if(at+2>blob.size())break;size_t count=(unsigned(blob[at])<<8)|blob[at+1];at+=2;if(count>blob.size()-at)break;auto t=spsTransfer(blob.subspan(at,count));if(t)return t;at+=count;}}
 for(size_t i=0;i+4<blob.size();i++)if(blob[i]==0&&blob[i+1]==0&&(blob[i+2]==1||(blob[i+2]==0&&blob[i+3]==1))){size_t start=i+(blob[i+2]==1?3:4),end=start;while(end+3<blob.size()&&!(blob[end]==0&&blob[end+1]==0&&(blob[end+2]==1||(blob[end+2]==0&&blob[end+3]==1))))end++;if(end+3>=blob.size())end=blob.size();auto t=spsTransfer(blob.subspan(start,end-start));if(t)return t;}
 return spsTransfer(blob);
}

}
