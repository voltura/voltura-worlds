#include "package.h"
#include "video_format.h"
#include "json.hpp"
#include <fstream>
#include <map>
#include <set>
#include <cstring>
#include <objbase.h>
#include <wincodec.h>
#include <wrl/client.h>
namespace worlds {
using Bytes=std::vector<unsigned char>;
static constexpr size_t limit=100ull*1024*1024;
static uint32_t crc(const Bytes& b){uint32_t c=~0u;for(auto v:b){c^=v;for(int i=0;i<8;i++)c=(c>>1)^(0xedb88320u&uint32_t(-int(c&1)));}return ~c;}
static uint16_t u16(const Bytes& b,size_t i){if(i+2>b.size())throw std::runtime_error("Truncated package");return uint16_t(b[i]|(b[i+1]<<8));}
static uint32_t u32(const Bytes& b,size_t i){return uint32_t(u16(b,i))|(uint32_t(u16(b,i+2))<<16);}
static void put16(Bytes& b,uint16_t v){b.push_back((unsigned char)v);b.push_back((unsigned char)(v>>8));}
static void put32(Bytes& b,uint32_t v){put16(b,uint16_t(v));put16(b,uint16_t(v>>16));}
static Bytes read(const std::filesystem::path& f){auto n=std::filesystem::file_size(f);if(n>limit)throw std::runtime_error("World package exceeds 100 MiB");Bytes b((size_t)n);std::ifstream s(f,std::ios::binary);if(!s.read(reinterpret_cast<char*>(b.data()),(std::streamsize)n))throw std::runtime_error("Cannot read World package");return b;}
void validateWorldPreview(const Bytes& b){
 static const unsigned char magic[]={137,80,78,71,13,10,26,10};
 if(b.size()<33||b.size()>8*1024*1024||memcmp(b.data(),magic,8))throw std::runtime_error("Preview must be a PNG under 8 MiB");
 auto be=[&](size_t i){return (uint32_t(b[i])<<24)|(uint32_t(b[i+1])<<16)|(uint32_t(b[i+2])<<8)|b[i+3];};
 if(be(8)!=13||memcmp(b.data()+12,"IHDR",4))throw std::runtime_error("Invalid PNG header");
 auto w=be(16),h=be(20);if(!w||!h||w>4096||h>4096)throw std::runtime_error("Preview dimensions exceed 4096 pixels");
 // Check the entire container before asking the system decoder to allocate pixels.
 bool ended=false;size_t position=8;
 while(position<b.size()){
  if(b.size()-position<12)throw std::runtime_error("Truncated PNG chunk");
  size_t length=be(position);if(length>b.size()-position-12)throw std::runtime_error("Invalid PNG chunk size");
  Bytes chunk(b.begin()+position+4,b.begin()+position+8+length);
  if(crc(chunk)!=be(position+8+length))throw std::runtime_error("PNG checksum failed");
  bool end=memcmp(b.data()+position+4,"IEND",4)==0;position+=length+12;
  if(end){if(length||position!=b.size())throw std::runtime_error("Unexpected trailing PNG data");ended=true;break;}
 }
 if(!ended)throw std::runtime_error("Incomplete PNG");
 struct Apartment {HRESULT hr=CoInitializeEx(nullptr,COINIT_MULTITHREADED);~Apartment(){if(SUCCEEDED(hr))CoUninitialize();}} apartment;
 if(FAILED(apartment.hr)&&apartment.hr!=RPC_E_CHANGED_MODE)throw std::runtime_error("Cannot initialize image validation");
 using Microsoft::WRL::ComPtr;ComPtr<IWICImagingFactory> factory;ComPtr<IWICStream> stream;ComPtr<IWICBitmapDecoder> decoder;ComPtr<IWICBitmapFrameDecode> frame;ComPtr<IWICFormatConverter> converter;
 auto checked=[](HRESULT hr){if(FAILED(hr))throw std::runtime_error("Preview PNG cannot be decoded");};
 checked(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)));
 checked(factory->CreateStream(&stream));checked(stream->InitializeFromMemory(const_cast<BYTE*>(b.data()),static_cast<DWORD>(b.size())));
 checked(factory->CreateDecoderFromStream(stream.Get(),nullptr,WICDecodeMetadataCacheOnLoad,&decoder));UINT count{};checked(decoder->GetFrameCount(&count));if(count!=1)throw std::runtime_error("Preview must contain one image");
 checked(decoder->GetFrame(0,&frame));UINT decodedW{},decodedH{};checked(frame->GetSize(&decodedW,&decodedH));if(decodedW!=w||decodedH!=h)throw std::runtime_error("Inconsistent PNG dimensions");
 checked(factory->CreateFormatConverter(&converter));checked(converter->Initialize(frame.Get(),GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom));
 // Decode every row, while keeping the validation buffer bounded to 64 KiB.
 Bytes row(size_t(w)*4);for(UINT y=0;y<h;y++){WICRect area{0,static_cast<INT>(y),static_cast<INT>(w),1};checked(converter->CopyPixels(&area,w*4,static_cast<UINT>(row.size()),row.data()));}
}
static void validate(const WorldPackage& p){
 if(p.title.empty()||p.title.size()>120||p.description.size()>4000||p.attribution.size()>2000||!valid(p.assignment.params))throw std::runtime_error("Invalid World metadata");
 validateWorldPreview(p.preview);if(p.assignment.scene==Scene::Video){if(!legacyAvcMp4(p.video))throw std::runtime_error("World version 1 video must be an H.264 MP4.");}else if(!p.video.empty())throw std::runtime_error("Preset cannot contain video");
 if(int(p.assignment.reaction)<0||int(p.assignment.reaction)>2)throw std::runtime_error("Invalid reaction");
}
WorldPackage readPackage(const std::filesystem::path& file){
 auto b=read(file);if(b.size()<22)throw std::runtime_error("Incomplete package");size_t end=b.size()-22;
 if(u32(b,end)!=0x06054b50||u16(b,end+4)||u16(b,end+6)||u16(b,end+20))throw std::runtime_error("Unsupported ZIP structure");
 unsigned entries=u16(b,end+10);if(entries<2||entries>3||u16(b,end+8)!=entries)throw std::runtime_error("Unexpected package entries");
 size_t central=u32(b,end+16),centralSize=u32(b,end+12);if(central+centralSize!=end)throw std::runtime_error("Invalid package directory");
 std::map<std::string,Bytes> files;size_t pos=central,lastLocal=0;
 for(unsigned i=0;i<entries;i++){
  if(pos+46> end||u32(b,pos)!=0x02014b50||u16(b,pos+8)||u16(b,pos+10)||u16(b,pos+30)||u16(b,pos+32)||u16(b,pos+34))throw std::runtime_error("Only plain stored ZIP entries are supported");
  size_t size=u32(b,pos+24),nameLen=u16(b,pos+28),offset=u32(b,pos+42);if(size!=u32(b,pos+20)||pos+46+nameLen>end||offset!=lastLocal)throw std::runtime_error("Invalid package entry");
  std::string name(reinterpret_cast<const char*>(b.data()+pos+46),nameLen);if(name!="manifest.json"&&name!="preview.png"&&name!="video.mp4")throw std::runtime_error("Unexpected or unsafe file in package");
  if(files.contains(name)||offset+30>central||u32(b,offset)!=0x04034b50||u16(b,offset+6)||u16(b,offset+8)||u16(b,offset+28)||u16(b,offset+26)!=nameLen)throw std::runtime_error("Invalid local file entry");
  size_t start=offset+30+nameLen;if(start+size>central||u32(b,offset+18)!=size||u32(b,offset+22)!=size||u32(b,offset+14)!=u32(b,pos+16)||std::string(reinterpret_cast<const char*>(b.data()+offset+30),nameLen)!=name)throw std::runtime_error("Inconsistent package directory");
  Bytes data(b.begin()+start,b.begin()+start+size);if(crc(data)!=u32(b,pos+16))throw std::runtime_error("Package checksum failed");files.emplace(name,std::move(data));lastLocal=start+size;pos+=46+nameLen;
 }
 if(pos!=end||lastLocal!=central||!files.contains("manifest.json")||!files.contains("preview.png")||files.at("manifest.json").size()>65536)throw std::runtime_error("Invalid package layout");
 auto j=nlohmann::json::parse(files.at("manifest.json"));if(j.at("format")!="voltura-world"||j.at("version")!=1)throw std::runtime_error("Unsupported World version");
 const std::set<std::string> keys={"format","version","title","description","attribution","scene","reaction","parameters","fit"};for(auto it=j.begin();it!=j.end();++it)if(!keys.contains(it.key()))throw std::runtime_error("Unknown manifest field");
 WorldPackage p;p.title=j.at("title");p.description=j.value("description",std::string{});p.attribution=j.value("attribution",std::string{});p.assignment.scene=parseScene(j.at("scene"));int reaction=j.at("reaction");if(reaction<0||reaction>2)throw std::runtime_error("Invalid reaction");p.assignment.reaction=Reaction(reaction);p.assignment.fit=j.value("fit",false);
 auto values=j.at("parameters");const std::set<std::string> parameterKeys={"brightness","movement","detail","flow","turbulence","density","strength","palette"};for(auto it=values.begin();it!=values.end();++it)if(!parameterKeys.contains(it.key()))throw std::runtime_error("Unknown scene parameter");
 p.assignment.params={values.at("brightness"),values.at("movement"),values.at("detail"),values.at("flow"),values.at("turbulence"),values.at("density"),values.at("strength"),values.at("palette")};
 p.preview=std::move(files.at("preview.png"));if(files.contains("video.mp4"))p.video=std::move(files.at("video.mp4"));validate(p);return p;
}
void writePackage(const std::filesystem::path& file,const WorldPackage& p){
 if(p.assignment.scene==Scene::Video&&p.assignment.videoSettings!=VideoSettings{})throw std::runtime_error("Video positioning and speed require World package version 2.");
 if(p.assignment.scene==Scene::Video&&!legacyAvcMp4(p.video))throw std::runtime_error("World version 1 supports only SDR H.264 MP4 video.");
 validate(p);const auto& a=p.assignment;const auto& v=a.params;nlohmann::json j={{"format","voltura-world"},{"version",1},{"title",p.title},{"description",p.description},{"attribution",p.attribution},{"scene",sceneName(a.scene)},{"reaction",int(a.reaction)},{"fit",a.fit},{"parameters",{{"brightness",v.brightness},{"movement",v.movement},{"detail",v.detail},{"flow",v.flow},{"turbulence",v.turbulence},{"density",v.density},{"strength",v.strength},{"palette",v.palette}}}};
 auto text=j.dump(2);std::map<std::string,Bytes> files{{"manifest.json",Bytes(text.begin(),text.end())},{"preview.png",p.preview}};if(a.scene==Scene::Video)files["video.mp4"]=p.video;
 Bytes b,central;for(auto& [name,data]:files){uint32_t offset=(uint32_t)b.size(),size=(uint32_t)data.size(),checksum=crc(data);
  put32(b,0x04034b50);put16(b,20);put16(b,0);put16(b,0);put16(b,0);put16(b,0);put32(b,checksum);put32(b,size);put32(b,size);put16(b,(uint16_t)name.size());put16(b,0);b.insert(b.end(),name.begin(),name.end());b.insert(b.end(),data.begin(),data.end());
  put32(central,0x02014b50);put16(central,20);put16(central,20);put16(central,0);put16(central,0);put16(central,0);put16(central,0);put32(central,checksum);put32(central,size);put32(central,size);put16(central,(uint16_t)name.size());put16(central,0);put16(central,0);put16(central,0);put16(central,0);put32(central,0);put32(central,offset);central.insert(central.end(),name.begin(),name.end());
 }
 uint32_t offset=(uint32_t)b.size();b.insert(b.end(),central.begin(),central.end());put32(b,0x06054b50);put16(b,0);put16(b,0);put16(b,(uint16_t)files.size());put16(b,(uint16_t)files.size());put32(b,(uint32_t)central.size());put32(b,offset);put16(b,0);if(b.size()>limit)throw std::runtime_error("Package exceeds 100 MiB");
 auto tmp=file;tmp+=L".pending";{std::ofstream out(tmp,std::ios::binary|std::ios::trunc);if(!out.write(reinterpret_cast<const char*>(b.data()),b.size()))throw std::runtime_error("Cannot write package");}
 if(!MoveFileExW(tmp.c_str(),file.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){std::filesystem::remove(tmp);throw std::runtime_error("Cannot commit package");}
}
Assignment installPackage(const std::filesystem::path& library,const WorldPackage& p){
 validate(p);std::filesystem::create_directories(library);GUID id{};if(FAILED(CoCreateGuid(&id)))throw std::runtime_error("Cannot allocate library identifier");wchar_t name[40];StringFromGUID2(id,name,40);auto stage=library/(std::wstring(name)+L".pending"),dest=library/name;
 std::filesystem::create_directory(stage);try{writePackage(stage/L"source.world",p);if(p.assignment.scene==Scene::Video){std::ofstream out(stage/L"video.mp4",std::ios::binary);if(!out.write(reinterpret_cast<const char*>(p.video.data()),p.video.size()))throw std::runtime_error("Cannot import video");}std::filesystem::rename(stage,dest);}catch(...){std::error_code ec;std::filesystem::remove_all(stage,ec);throw;}
 auto a=p.assignment;if(a.scene==Scene::Video)a.video=(dest/L"video.mp4").wstring();return a;
}
}
