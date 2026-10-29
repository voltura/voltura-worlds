#include "package.h"
#include "random_cycle.h"
#include "video_format.h"
#include "json.hpp"
#include <bcrypt.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <fstream>
#include <map>
#include <set>
#include <array>
#include <algorithm>
#include <cwctype>
namespace worlds {
namespace {
using Json=nlohmann::json;
constexpr uint64_t maximum=100ull*1024*1024;
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
void cancelled(std::stop_token stop){if(stop.stop_requested())throw std::runtime_error("Operation cancelled.");}
uint32_t checksum(uint32_t crc,const char* bytes,size_t n){for(size_t i=0;i<n;i++){crc^=static_cast<unsigned char>(bytes[i]);for(int k=0;k<8;k++)crc=(crc>>1)^(0xedb88320u&uint32_t(-int(crc&1)));}return crc;}
uint32_t get(std::istream& in,int n){uint32_t value=0;for(int i=0;i<n;i++){int c=in.get();require(c!=EOF,"Truncated World package.");value|=uint32_t(c)<<(i*8);}return value;}
void put(std::ostream& out,uint32_t value,int n){for(int i=0;i<n;i++)out.put(char(value>>(i*8)));}
struct Entry {std::string name;uint32_t size{},crc{},local{},offset{};};
std::vector<Entry> directory(const std::filesystem::path& file,std::stop_token stop,bool verify=true){
 auto length=std::filesystem::file_size(file);require(length>=22&&length<=maximum,"World package must be under 100 MiB.");
 std::ifstream in(file,std::ios::binary);in.seekg(length-22);require(get(in,4)==0x06054b50&&get(in,2)==0&&get(in,2)==0,"Unsupported ZIP structure.");
 auto count=get(in,2);require(count>=2&&count<=34&&get(in,2)==count,"Invalid package entry count.");auto centralSize=get(in,4),central=get(in,4);require(get(in,2)==0&&uint64_t(central)+centralSize==length-22,"Invalid ZIP directory.");
 std::vector<Entry> entries;std::set<std::string> names;uint64_t previous=0;in.seekg(central);
 for(unsigned i=0;i<count;i++){
  require(get(in,4)==0x02014b50,"Invalid ZIP entry.");get(in,2);get(in,2);require(get(in,2)==0&&get(in,2)==0,"Only stored ZIP entries are supported.");get(in,2);get(in,2);
  Entry e;e.crc=get(in,4);e.size=get(in,4);require(get(in,4)==e.size,"Invalid ZIP size.");auto n=get(in,2);require(n>0&&n<=80&&get(in,2)==0&&get(in,2)==0&&get(in,2)==0,"Invalid ZIP metadata.");get(in,2);get(in,4);e.local=get(in,4);
  e.name.resize(n);in.read(e.name.data(),n);require(bool(in)&&names.insert(e.name).second,"Duplicate package entry.");
  bool media=e.name.starts_with("media/")&&e.name.size()==70&&std::all_of(e.name.begin()+6,e.name.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');});
  require(e.name=="manifest.json"||e.name=="preview.png"||e.name=="video.mp4"||media,"Unsafe or unexpected package path.");
  auto next=in.tellg();require(e.local==previous&&uint64_t(e.local)+30+n+e.size<=central,"Invalid local entry bounds.");in.seekg(e.local);
  require(get(in,4)==0x04034b50,"Invalid local entry.");get(in,2);require(get(in,2)==0&&get(in,2)==0,"Invalid local flags.");get(in,2);get(in,2);
  require(get(in,4)==e.crc&&get(in,4)==e.size&&get(in,4)==e.size&&get(in,2)==n&&get(in,2)==0,"Inconsistent local entry.");std::string name(n,'\0');in.read(name.data(),n);require(name==e.name,"Inconsistent entry name.");e.offset=e.local+30+n;
  if(verify){uint32_t crc=~0u;std::array<char,65536> buffer{};uint64_t remaining=e.size;while(remaining){cancelled(stop);auto bytes=std::min<uint64_t>(remaining,buffer.size());in.read(buffer.data(),bytes);require(bool(in),"Truncated package data.");crc=checksum(crc,buffer.data(),size_t(bytes));remaining-=bytes;}require(~crc==e.crc,"Package checksum failed.");}previous=uint64_t(e.offset)+e.size;entries.push_back(e);in.seekg(next);
 }
 require(in.tellg()==std::streampos(length-22)&&previous==central&&names.contains("manifest.json")&&names.contains("preview.png"),"Invalid package layout.");return entries;
}
std::string contents(const std::filesystem::path& file,const Entry& entry,size_t limit){require(entry.size<=limit,"Package metadata exceeds limit.");std::ifstream in(file,std::ios::binary);in.seekg(entry.offset);std::string data(entry.size,'\0');in.read(data.data(),data.size());require(bool(in),"Cannot read package metadata.");require(~checksum(~0u,data.data(),data.size())==entry.crc,"Package metadata changed or its checksum failed.");return data;}
void fields(const Json& j,std::initializer_list<const char*> allowed){require(j.is_object(),"Expected an object.");for(auto it=j.begin();it!=j.end();++it)require(std::any_of(allowed.begin(),allowed.end(),[&](const char* s){return it.key()==s;}),"Unknown preset field.");}
Json assignmentJson(const Assignment& a){
 const auto& p=a.params;
 return {{"scene",sceneName(a.scene)},{"parameters",{{"brightness",p.brightness},{"movement",p.movement},{"detail",p.detail},{"flow",p.flow},{"turbulence",p.turbulence},{"density",p.density},{"strength",p.strength},{"palette",p.palette}}},{"reaction",int(a.reaction)},{"quality",int(a.performance.quality)},{"frameLimit",int(a.performance.frameLimit)},{"output",int(a.output)},{"fit",a.fit},{"videoSettings",{{"positionX",a.videoSettings.positionX},{"positionY",a.videoSettings.positionY},{"speed",a.videoSettings.speed}}},{"random",a.random},{"randomInterval",a.randomInterval},{"randomPool",int(a.randomPool)},{"showBodyStatistics",a.showBodyStatistics},{"journeyShowBodyStatistics",a.journeyShowBodyStatistics}};
}
Assignment parseAssignment(const Json& j){
 fields(j,{"scene","parameters","reaction","quality","frameLimit","output","fit","videoSettings","random","randomInterval","randomPool","showBodyStatistics","journeyShowBodyStatistics"});Assignment a;a.scene=parseScene(j.at("scene"));auto& p=j.at("parameters");fields(p,{"brightness","movement","detail","flow","turbulence","density","strength","palette"});require(p.at("palette").is_number_integer()&&p.at("palette")>=0&&p.at("palette")<=3,"Invalid preset palette.");a.params={p.at("brightness"),p.at("movement"),p.at("detail"),p.at("flow"),p.at("turbulence"),p.at("density"),p.at("strength"),p.at("palette")};
 auto enumeration=[&](const char* key,int max){require(j.at(key).is_number_integer(),"Preset enum must be an integer.");auto n=j.at(key).get<int64_t>();require(n>=0&&n<=max,"Invalid preset enum.");return int(n);};a.reaction=Reaction(enumeration("reaction",2));a.performance={Quality(enumeration("quality",2)),FrameLimit(enumeration("frameLimit",2))};a.output=OutputPreference(enumeration("output",1));a.fit=j.at("fit");auto& v=j.at("videoSettings");fields(v,{"positionX","positionY","speed"});a.videoSettings={v.at("positionX"),v.at("positionY"),v.at("speed")};a.random=j.at("random");require(j.at("randomInterval").is_number_integer()&&p.at("palette").is_number_integer(),"Preset interval and palette must be integers.");auto interval=j.at("randomInterval").get<int64_t>();require(interval>=0&&interval<=1800,"Invalid preset interval.");a.randomInterval=int(interval);a.randomPool=RandomPool(enumeration("randomPool",1));a.showBodyStatistics=j.at("showBodyStatistics");a.journeyShowBodyStatistics=j.value("journeyShowBodyStatistics",true);
 require(p.at("palette").get<int64_t>()>=0&&p.at("palette").get<int64_t>()<=3,"Invalid preset palette.");require(valid(a.params)&&validVideoSettings(a.videoSettings)&&validRandomInterval(a.randomInterval),"Invalid preset configuration.");require(!a.random||validRandomScene(a.scene,a.randomPool),"Scene is outside the selected random pool.");return a;
}
void validate(const DesktopPreset& p){
 validatePresetName(p.title);require(p.description.size()<=4000&&p.attribution.size()<=2000,"Preset description or attribution is too long.");require(p.description.find('\0')==std::string::npos&&p.attribution.find('\0')==std::string::npos,"Preset metadata contains a null character.");wide(p.description);wide(p.attribution);require(p.audioDevice.find(L'\0')==std::wstring::npos&&utf8(p.audioDevice).size()<=4096,"Invalid audio device identifier.");require(!p.displays.empty()&&p.displays.size()<=32,"A preset must contain 1–32 displays.");std::set<std::wstring> ids;
 for(auto& d:p.displays){require(!d.id.empty()&&d.id.find(L'\0')==std::wstring::npos&&d.label.find(L'\0')==std::wstring::npos&&utf8(d.id).size()<=4096&&utf8(d.label).size()<=1024&&ids.insert(d.id).second,"Invalid or duplicate display identifier.");parseAssignment(assignmentJson(d.assignment));}
}
Json manifest(const DesktopPreset& p){Json j={{"format","voltura-world"},{"version",2},{"title",p.title},{"description",p.description},{"attribution",p.attribution},{"pauseBattery",p.pauseBattery},{"audioDevice",utf8(p.audioDevice)},{"displays",Json::array()}};for(auto& d:p.displays)j["displays"].push_back({{"id",utf8(d.id)},{"label",utf8(d.label)},{"assignment",assignmentJson(d.assignment)},{"media",d.media}});return j;}
struct FileRange {std::filesystem::path file;uint64_t offset{},size{};};
template<class F> void stream(const FileRange& range,std::stop_token stop,F consume){std::ifstream in(range.file,std::ios::binary);require(bool(in),"Preset media is missing or unreadable.");auto size=std::filesystem::file_size(range.file);require(range.offset<=size&&range.size<=size-range.offset,"Media changed while preparing preset.");in.seekg(range.offset);std::array<char,65536> buffer{};uint64_t remaining=range.size;while(remaining){cancelled(stop);auto n=size_t(std::min<uint64_t>(remaining,buffer.size()));in.read(buffer.data(),n);require(bool(in),"Cannot read preset media.");consume(buffer.data(),n);remaining-=n;}}
class MediaHash {
 BCRYPT_ALG_HANDLE algorithm_{};BCRYPT_HASH_HANDLE hash_{};
public:
 MediaHash(){
  require(BCryptOpenAlgorithmProvider(&algorithm_,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0,"Cannot initialize media hash.");
  if(BCryptCreateHash(algorithm_,&hash_,nullptr,0,nullptr,0,0)<0){BCryptCloseAlgorithmProvider(algorithm_,0);throw std::runtime_error("Cannot create media hash.");}
 }
 MediaHash(const MediaHash&)=delete;MediaHash& operator=(const MediaHash&)=delete;
 ~MediaHash(){if(hash_)BCryptDestroyHash(hash_);if(algorithm_)BCryptCloseAlgorithmProvider(algorithm_,0);}
 void add(const char* bytes,size_t size){require(BCryptHashData(hash_,reinterpret_cast<PUCHAR>(const_cast<char*>(bytes)),ULONG(size),0)>=0,"Cannot hash media.");}
 std::string finish(){
  unsigned char bytes[32];require(BCryptFinishHash(hash_,bytes,32,0)>=0,"Cannot finish media hash.");
  std::string value;value.reserve(64);for(auto b:bytes){value+="0123456789abcdef"[b>>4];value+="0123456789abcdef"[b&15];}return value;
 }
};
std::string digest(const FileRange& range,std::stop_token stop){MediaHash hash;stream(range,stop,[&](const char* bytes,size_t size){hash.add(bytes,size);});return hash.finish();}

std::vector<unsigned char> summary(const DesktopPreset& preset){
 // A configuration summary, deliberately independent of live renderer/HDR capture.
 using Microsoft::WRL::ComPtr;auto check=[](HRESULT hr){require(SUCCEEDED(hr),"Cannot create preset summary.");};
 struct Drawing {
  HDC dc{};HBITMAP bitmap{};HFONT font{};HGDIOBJ oldBitmap{},oldFont{};
  ~Drawing(){if(dc){if(oldFont&&oldFont!=HGDI_ERROR)SelectObject(dc,oldFont);if(oldBitmap&&oldBitmap!=HGDI_ERROR)SelectObject(dc,oldBitmap);}if(font)DeleteObject(font);if(bitmap)DeleteObject(bitmap);if(dc)DeleteDC(dc);}
 } drawing;
 HDC dc=drawing.dc=CreateCompatibleDC(nullptr);require(dc!=nullptr,"Cannot create summary drawing context.");int height=100+int(preset.displays.size())*36;BITMAPINFO info{};info.bmiHeader={sizeof(BITMAPINFOHEADER),960,-height,1,32,BI_RGB};void* pixels{};HBITMAP bitmap=drawing.bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,nullptr,0);require(bitmap!=nullptr,"Cannot create summary bitmap.");drawing.oldBitmap=SelectObject(dc,bitmap);require(drawing.oldBitmap&&drawing.oldBitmap!=HGDI_ERROR,"Cannot select summary bitmap.");auto brush=CreateSolidBrush(RGB(20,28,40));RECT r{0,0,960,height};FillRect(dc,&r,brush);DeleteObject(brush);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(230,240,245));HFONT font=drawing.font=CreateFontW(-22,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,0,0,L"Segoe UI");require(font!=nullptr,"Cannot create summary font.");drawing.oldFont=SelectObject(dc,font);require(drawing.oldFont&&drawing.oldFont!=HGDI_ERROR,"Cannot select summary font.");auto title=wide(preset.title);TextOutW(dc,24,22,title.c_str(),int(title.size()));int y=78;for(auto& d:preset.displays){auto label=d.label+L" — "+(d.assignment.random?L"Random":wide(sceneName(d.assignment.scene)));RECT line{24,y,936,y+32};DrawTextW(dc,label.c_str(),-1,&line,DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX);y+=36;}GdiFlush();
 std::vector<unsigned char> result;
 {ComPtr<IWICImagingFactory> factory;ComPtr<IStream> output;ComPtr<IWICBitmapEncoder> encoder;ComPtr<IWICBitmapFrameEncode> frame;check(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)));check(CreateStreamOnHGlobal(nullptr,TRUE,&output));check(factory->CreateEncoder(GUID_ContainerFormatPng,nullptr,&encoder));check(encoder->Initialize(output.Get(),WICBitmapEncoderNoCache));check(encoder->CreateNewFrame(&frame,nullptr));check(frame->Initialize(nullptr));check(frame->SetSize(960,height));WICPixelFormatGUID format=GUID_WICPixelFormat32bppBGR;check(frame->SetPixelFormat(&format));ComPtr<IWICBitmap> raster;check(factory->CreateBitmapFromMemory(960,height,GUID_WICPixelFormat32bppBGR,960*4,960*height*4,static_cast<BYTE*>(pixels),&raster));check(frame->WriteSource(raster.Get(),nullptr));check(frame->Commit());check(encoder->Commit());STATSTG stat{};check(output->Stat(&stat,STATFLAG_NONAME));require(stat.cbSize.QuadPart<=8*1024*1024,"Summary is too large.");result.resize(size_t(stat.cbSize.QuadPart));LARGE_INTEGER zero{};check(output->Seek(zero,STREAM_SEEK_SET,nullptr));ULONG read{};check(output->Read(result.data(),ULONG(result.size()),&read));require(read==result.size(),"Cannot read summary PNG.");}
 return result;
}
void validatePng(const std::string& bytes){validateWorldPreview(std::vector<unsigned char>(bytes.begin(),bytes.end()));}
}
std::string newPresetId(){GUID id{};require(SUCCEEDED(CoCreateGuid(&id)),"Cannot allocate preset identifier.");wchar_t text[40]{};StringFromGUID2(id,text,40);return utf8(text);}
void validatePresetName(const std::string& name){auto value=wide(name);require(!name.empty()&&name.size()<=120&&std::any_of(value.begin(),value.end(),[](wchar_t c){return !iswspace(c);})&&std::none_of(value.begin(),value.end(),[](wchar_t c){return c<32;}),"Enter a nonblank preset name of at most 120 UTF-8 bytes.");}
bool samePresetName(const std::string& a,const std::string& b){auto x=wide(a),y=wide(b);return CompareStringOrdinal(x.data(),int(x.size()),y.data(),int(y.size()),TRUE)==CSTR_EQUAL;}
DesktopPreset snapshotPreset(const Settings& settings,const std::vector<Monitor>& monitors){DesktopPreset p;p.title="My desktop";p.audioDevice=settings.audioDevice;p.pauseBattery=settings.pauseBattery;for(auto& m:monitors){auto a=assignmentFor(settings,m.id);std::string media;if(a.scene==Scene::Video){media=std::to_string(p.media.size());p.media.push_back({media,a.video,0,std::filesystem::file_size(a.video)});}p.displays.push_back({m.id,m.name,a,media});}validate(p);return p;}
bool isDesktopPackage(const std::filesystem::path& file){try{auto entries=directory(file,{},false);for(auto& e:entries)if(e.name=="manifest.json")return Json::parse(contents(file,e,65536)).at("version")==2;}catch(const std::exception&){}return false;}
DesktopPreset readDesktopPackage(const std::filesystem::path& file,std::stop_token stop){
 auto entries=directory(file,stop);DesktopPreset p;Json j;for(auto& e:entries){if(e.name=="manifest.json")j=Json::parse(contents(file,e,65536));else if(e.name=="preview.png")validatePng(contents(file,e,8*1024*1024));else {require(e.name.starts_with("media/"),"Unexpected version-2 media.");p.media.push_back({e.name,file,e.offset,e.size});}}
 fields(j,{"format","version","title","description","attribution","pauseBattery","audioDevice","displays"});require(j.at("format")=="voltura-world"&&j.at("version")==2,"Unsupported desktop preset version.");p.title=j.at("title");p.description=j.at("description");p.attribution=j.at("attribution");p.pauseBattery=j.at("pauseBattery");p.audioDevice=wide(j.at("audioDevice"));require(j.at("displays").is_array()&&j.at("displays").size()<=32,"Too many preset displays.");std::set<std::string> used;
 for(auto& d:j.at("displays")){fields(d,{"id","label","assignment","media"});PresetDisplay display{wide(d.at("id")),wide(d.at("label")),parseAssignment(d.at("assignment")),d.at("media")};if(display.assignment.scene==Scene::Video){require(std::any_of(p.media.begin(),p.media.end(),[&](auto& m){return m.name==display.media;}),"Missing video asset.");used.insert(display.media);}else require(display.media.empty(),"Unexpected video reference.");p.displays.push_back(std::move(display));}
 require(used.size()==p.media.size(),"Unreferenced package media.");for(auto& m:p.media){cancelled(stop);require(m.name=="media/"+digest({m.source,m.offset,m.size},stop),"Media identity mismatch.");}validate(p);return p;
}
void writeDesktopPackage(const std::filesystem::path& file,const DesktopPreset& source,std::stop_token stop,PresetProgress progress){
 validate(source);cancelled(stop);DesktopPreset p=source;std::map<std::string,FileRange> media;std::map<std::string,std::string> names;
 for(auto& m:p.media){require(!names.contains(m.name),"Duplicate media reference.");require(m.size>0&&m.size<=maximum,"Video exceeds package size limit.");if(progress)progress(L"Reading and deduplicating video…");FileRange range{m.source,m.offset,m.size};auto name="media/"+digest(range,stop);require(!m.name.starts_with("media/")||m.name==name,"Preset media changed since validation; reopen the preset.");names.emplace(m.name,name);media.emplace(name,range);}
 std::set<std::string> used;for(auto& d:p.displays){if(d.assignment.scene==Scene::Video){require(names.contains(d.media),"Missing preset video.");d.media=names.at(d.media);used.insert(d.media);}else require(d.media.empty(),"Unexpected media reference.");}
 require(used.size()==media.size(),"Unreferenced preset video.");auto text=manifest(p).dump(2);require(text.size()<=65536,"Preset manifest exceeds 64 KiB.");auto png=summary(p);
 struct Output {std::string name,data;FileRange range;bool media=false;uint32_t crc{},size{},local{};};std::vector<Output> files{{"manifest.json",text},{"preview.png",std::string(png.begin(),png.end())}};for(auto& [name,range]:media){Output output;output.name=name;output.range=range;output.media=true;files.push_back(output);}
 uint64_t total=22;for(auto& e:files){e.size=uint32_t(e.media?e.range.size:e.data.size());total+=76+2*e.name.size()+e.size;require(total<=maximum,"World package exceeds 100 MiB. Choose smaller videos.");uint32_t crc=~0u;if(e.media)stream(e.range,stop,[&](const char* b,size_t n){crc=checksum(crc,b,n);});else crc=checksum(crc,e.data.data(),e.data.size());e.crc=~crc;}
 auto pending=file;pending+=L"."+wide(newPresetId())+L".pending";
 try{std::ofstream out(pending,std::ios::binary|std::ios::trunc);require(bool(out),"Cannot create export file.");for(auto& e:files){cancelled(stop);if(progress)progress(L"Writing "+wide(e.name)+L"…");e.local=uint32_t(out.tellp());put(out,0x04034b50,4);put(out,20,2);put(out,0,2);put(out,0,2);put(out,0,2);put(out,0,2);put(out,e.crc,4);put(out,e.size,4);put(out,e.size,4);put(out,uint32_t(e.name.size()),2);put(out,0,2);out.write(e.name.data(),e.name.size());uint32_t writtenCrc=~0u;if(e.media){MediaHash writtenHash;stream(e.range,stop,[&](const char* b,size_t n){out.write(b,n);writtenCrc=checksum(writtenCrc,b,n);writtenHash.add(b,n);require(bool(out),"Cannot write export media.");});require(e.name=="media/"+writtenHash.finish(),"Media changed during export; try again.");}else{out.write(e.data.data(),e.data.size());writtenCrc=checksum(writtenCrc,e.data.data(),e.data.size());}require(~writtenCrc==e.crc,"Media changed during export; try again.");}
 auto central=uint32_t(out.tellp());for(auto& e:files){put(out,0x02014b50,4);put(out,20,2);put(out,20,2);for(int k=0;k<4;k++)put(out,0,2);put(out,e.crc,4);put(out,e.size,4);put(out,e.size,4);put(out,uint32_t(e.name.size()),2);for(int k=0;k<4;k++)put(out,0,2);put(out,0,4);put(out,e.local,4);out.write(e.name.data(),e.name.size());}auto centralSize=uint32_t(out.tellp())-central;put(out,0x06054b50,4);put(out,0,2);put(out,0,2);put(out,uint32_t(files.size()),2);put(out,uint32_t(files.size()),2);put(out,centralSize,4);put(out,central,4);put(out,0,2);out.flush();require(bool(out),"Cannot finish export.");out.close();require(bool(out),"Cannot close export file.");cancelled(stop);require(MoveFileExW(pending.c_str(),file.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0,"Cannot replace export file.");}
 catch(...){std::error_code ec;std::filesystem::remove(pending,ec);throw;}
}
DesktopPreset installDesktopMedia(const std::filesystem::path& library,const DesktopPreset& source,std::stop_token stop,PresetProgress progress){
 DesktopPreset p=source;validate(p);if(p.media.empty())return p;std::filesystem::create_directories(library);auto destination=library/wide(newPresetId()),stage=destination;stage+=L".pending";require(std::filesystem::create_directory(stage),"Import staging directory already exists.");
 try{for(size_t i=0;i<p.media.size();i++){cancelled(stop);if(progress)progress(L"Installing video "+std::to_wstring(i+1)+L"…");auto& m=p.media[i];std::ifstream headerFile(m.source,std::ios::binary);headerFile.seekg(m.offset);std::vector<unsigned char> header(size_t(std::min<uint64_t>(m.size,65536)));headerFile.read(reinterpret_cast<char*>(header.data()),header.size());require(bool(headerFile),"Cannot read video container.");auto container=videoContainer(header);require(!container.empty(),"Unrecognized video container.");auto name=std::to_wstring(i)+L"."+wide(container);std::ofstream out(stage/name,std::ios::binary);stream({m.source,m.offset,m.size},stop,[&](const char* b,size_t n){out.write(b,n);require(bool(out),"Cannot install preset media.");});out.close();require(bool(out),"Cannot finish preset media.");if(m.name.starts_with("media/"))require(m.name=="media/"+digest({stage/name,0,m.size},stop),"Preset media changed since validation.");for(auto& d:p.displays)if(d.media==m.name)d.assignment.video=(destination/name).wstring();m.source=destination/name;m.offset=0;}cancelled(stop);std::filesystem::rename(stage,destination);return p;}
 catch(...){std::error_code ec;std::filesystem::remove_all(stage,ec);throw;}
}
std::vector<SavedPreset> listPresets(const std::filesystem::path& root,std::stop_token stop){std::vector<SavedPreset> result;if(!std::filesystem::exists(root))return result;for(auto& entry:std::filesystem::directory_iterator(root)){cancelled(stop);if(entry.is_regular_file()&&_wcsicmp(entry.path().extension().c_str(),L".world")==0){try{result.push_back({entry.path(),readDesktopPackage(entry.path(),stop)});}catch(const std::exception& error){cancelled(stop);SavedPreset damaged;damaged.file=entry.path();damaged.preset.title=utf8(entry.path().stem().wstring());damaged.error=error.what();result.push_back(std::move(damaged));}}}std::sort(result.begin(),result.end(),[](auto& a,auto& b){return a.preset.title<b.preset.title;});return result;}
std::filesystem::path savePreset(const std::filesystem::path& root,const DesktopPreset& p,const std::filesystem::path& replace,std::stop_token stop,PresetProgress progress){validate(p);std::filesystem::create_directories(root);for(auto& entry:listPresets(root,stop))require(!entry.error.empty()||entry.file==replace||!samePresetName(entry.preset.title,p.title),"A preset with this name exists. Select it and choose Update.");auto file=replace.empty()?root/(wide(newPresetId())+L".world"):replace;require(file.parent_path()==root&&_wcsicmp(file.extension().c_str(),L".world")==0,"Invalid preset destination.");writeDesktopPackage(file,p,stop,progress);return file;}
}
