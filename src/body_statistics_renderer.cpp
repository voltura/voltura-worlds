#include "renderer.h"
#include "body_catalog.h"
#include "body_card_layout.h"
#include <gdiplus.h>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace worlds {
namespace {
struct GdiSession {
 ULONG_PTR token{};
 GdiSession(){Gdiplus::GdiplusStartupInput input;if(Gdiplus::GdiplusStartup(&token,&input,nullptr)!=Gdiplus::Ok)throw std::runtime_error("Cannot initialize body statistics typography");}
 ~GdiSession(){Gdiplus::GdiplusShutdown(token);}
};
Gdiplus::Color accentFor(const BodyFacts& body,int palette){
 auto c=body.accent;
 // Natural body colors follow the same warm/cool/neutral grades as the scenes.
 if(body.scene==Scene::Sun){constexpr unsigned char solar[4][3]={{255,165,98},{255,209,116},{255,140,97},{210,217,229}};for(int i=0;i<3;i++)c[i]=solar[std::clamp(palette,0,3)][i];}
 else if(body.scene==Scene::Uranus&&palette==1)c={165,226,218};
 else if(body.scene==Scene::Neptune&&palette==1)c={139,208,208};
 else if(palette==1){c[0]=BYTE(std::min(255,int(c[0])+16));c[2]=BYTE(c[2]*.88f);}
 else if(palette==2){c[0]=BYTE(c[0]*.88f);c[2]=BYTE(std::min(255,int(c[2])+24));}
 else if(palette==3)c={204,214,225};
 return Gdiplus::Color(255,c[0],c[1],c[2]);
}
void rounded(Gdiplus::GraphicsPath& path,float w,float h){
 constexpr float d=22;path.AddArc(1.f,1.f,d,d,180.f,90.f);path.AddArc(w-d-1,1.f,d,d,270.f,90.f);path.AddArc(w-d-1,h-d-1,d,d,0.f,90.f);path.AddArc(1.f,h-d-1,d,d,90.f,90.f);path.CloseFigure();
}
}

void Renderer::prepareBodyStatistics(const Assignment& assignment,int palette){
 const auto* body=bodyFacts(assignment.scene);
 if(!assignment.showBodyStatistics||!body){
  if(bodyStatisticsTexture_){idle();bodyStatisticsTexture_.Reset();bodyStatisticsBytes_=0;}
  return;
 }
 if(bodyStatisticsTexture_&&bodyStatisticsScene_==assignment.scene&&bodyStatisticsPalette_==palette&&bodyStatisticsOutputWidth_==width_&&bodyStatisticsOutputHeight_==height_&&bodyStatisticsInsets_==overlayInsets_)return;
 static GdiSession session;
 using namespace Gdiplus;
 const auto accent=accentFor(*body,palette);
 FontFamily family(L"Segoe UI");Font title(&family,26,FontStyleBold,UnitPixel),labelFont(&family,11,FontStyleRegular,UnitPixel),regular(&family,14,FontStyleRegular,UnitPixel),note(&family,13,FontStyleRegular,UnitPixel);
 SolidBrush white(Color(255,236,241,246)),muted(Color(255,169,184,199)),highlight(accent);
 StringFormat format(StringFormat::GenericTypographic());format.SetFormatFlags(StringFormatFlagsMeasureTrailingSpaces);
 // Measure and paint with the same wrapped layout, so longer facts never clip.
 auto layout=[&](Graphics& graphics,bool paint){
  float y=18;
  auto text=[&](const wchar_t* value,Font& font,SolidBrush& brush,float gap){
   RectF box(19,y,322,1000),measured;graphics.MeasureString(value,-1,&font,box,&format,&measured);
   if(paint)graphics.DrawString(value,-1,&font,RectF(19,y,322,measured.Height+2),&format,&brush);
   y+=measured.Height+gap;
  };
  text(body->name,title,white,3);text(body->classification,labelFont,highlight,12);
  if(paint){Pen rule(Color(65,accent.GetR(),accent.GetG(),accent.GetB()),1);graphics.DrawLine(&rule,19.f,y,341.f,y);}y+=13;
  text(L"MEAN DIAMETER",labelFont,muted,3);text(body->diameter,regular,white,10);
  text(L"DISTANCE FROM EARTH",labelFont,muted,3);text(body->earthDistance,regular,white,10);
  text(L"ATMOSPHERE",labelFont,muted,3);text(body->atmosphere,regular,white,13);
  text(body->characteristic1,note,white,6);text(body->characteristic2,note,white,0);
  return std::ceil(y+18);
 };
 Bitmap measureBitmap(1,1,PixelFormat32bppARGB);Graphics measure(&measureBitmap);measure.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
 const float logicalHeight=layout(measure,false);
 const auto placement=bodyCardPlacement(width_,height_,logicalHeight,overlayInsets_);
 const float scale=placement.scale;
 const unsigned w=std::max(1u,unsigned(placement.width)),h=std::max(1u,unsigned(placement.height));
 Bitmap bitmap(w,h,PixelFormat32bppARGB);
 {
  Graphics graphics(&bitmap);graphics.Clear(Color(0,0,0,0));graphics.ScaleTransform(scale,scale);graphics.SetSmoothingMode(SmoothingModeAntiAlias);graphics.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
  GraphicsPath outline;rounded(outline,360,logicalHeight);
  SolidBrush background(Color(221,BYTE(8+accent.GetR()/30),BYTE(12+accent.GetG()/30),BYTE(18+accent.GetB()/30)));
  graphics.FillPath(&background,&outline);Pen border(Color(100,accent.GetR(),accent.GetG(),accent.GetB()),1);graphics.DrawPath(&border,&outline);
  layout(graphics,true);
 }
 BitmapData pixels{};Rect bounds(0,0,w,h);
 if(bitmap.LockBits(&bounds,ImageLockModeRead,PixelFormat32bppARGB,&pixels)!=Ok)throw std::runtime_error("Cannot rasterize body statistics");
 std::vector<BYTE> data(size_t(w)*h*4);
 for(unsigned y=0;y<h;y++)std::memcpy(data.data()+size_t(y)*w*4,static_cast<BYTE*>(pixels.Scan0)+ptrdiff_t(y)*pixels.Stride,size_t(w)*4);
 bitmap.UnlockBits(&pixels);
 idle(); // The old texture and its descriptor may still be in flight.
 D3D12_RESOURCE_DESC desc{};desc.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;desc.Width=w;desc.Height=h;desc.DepthOrArraySize=1;desc.MipLevels=1;desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;desc.SampleDesc.Count=1;
 D3D12_HEAP_PROPERTIES hp{};hp.Type=D3D12_HEAP_TYPE_DEFAULT;ComPtr<ID3D12Resource> texture,upload;
 check(device_->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&desc,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&texture)),"Create body statistics texture");
 D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};UINT64 bytes{};device_->GetCopyableFootprints(&desc,0,1,0,&footprint,nullptr,nullptr,&bytes);
 D3D12_RESOURCE_DESC buffer{};buffer.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;buffer.Width=bytes;buffer.Height=1;buffer.DepthOrArraySize=1;buffer.MipLevels=1;buffer.SampleDesc.Count=1;buffer.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;hp.Type=D3D12_HEAP_TYPE_UPLOAD;
 check(device_->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&buffer,D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&upload)),"Create body statistics upload");
 BYTE* mapped{};D3D12_RANGE none{};check(upload->Map(0,&none,reinterpret_cast<void**>(&mapped)),"Map body statistics upload");
 for(unsigned y=0;y<h;y++)std::memcpy(mapped+footprint.Offset+size_t(y)*footprint.Footprint.RowPitch,data.data()+size_t(y)*w*4,size_t(w)*4);
 upload->Unmap(0,nullptr);
 begin();D3D12_TEXTURE_COPY_LOCATION source{},destination{};source.pResource=upload.Get();source.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;source.PlacedFootprint=footprint;destination.pResource=texture.Get();destination.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
 list_->CopyTextureRegion(&destination,0,0,0,&source,nullptr);transition(texture.Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);submit();idle();
 D3D12_SHADER_RESOURCE_VIEW_DESC srv{};srv.Format=desc.Format;srv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;srv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;srv.Texture2D.MipLevels=1;device_->CreateShaderResourceView(texture.Get(),&srv,cpu(417));
 bodyStatisticsTexture_=std::move(texture);bodyStatisticsBytes_=device_->GetResourceAllocationInfo(0,1,&desc).SizeInBytes;
 bodyStatisticsScene_=assignment.scene;bodyStatisticsPalette_=palette;bodyStatisticsOutputWidth_=width_;bodyStatisticsOutputHeight_=height_;bodyStatisticsWidth_=w;bodyStatisticsHeight_=h;
 bodyStatisticsInsets_=overlayInsets_;bodyStatisticsX_=placement.x;bodyStatisticsY_=placement.y;
}

void Renderer::drawBodyStatistics(const Assignment& assignment){
 if(!assignment.showBodyStatistics||!bodyFacts(assignment.scene)||!bodyStatisticsTexture_)return;
 D3D12_VIEWPORT viewport{bodyStatisticsX_,bodyStatisticsY_,float(bodyStatisticsWidth_),float(bodyStatisticsHeight_),0,1};list_->RSSetViewports(1,&viewport);
 list_->SetGraphicsRootDescriptorTable(3,gpu(417));outputConstants();list_->SetPipelineState(output_.hdr?bodyStatisticsHdr_.Get():bodyStatisticsPipeline_.Get());list_->DrawInstanced(3,1,0,0);
}
}
