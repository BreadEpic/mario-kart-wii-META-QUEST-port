// SPDX-License-Identifier: GPL-3.0-or-later
#include "../lib/gfx/cockpit.hpp"
#include <fstream>
#include <iostream>
#include <atomic>
namespace aurora::webgpu { wgpu::Device g_device; GraphicsConfig g_graphicsConfig{}; }
std::atomic<int> errors=0;
int main() {
  using namespace aurora;
  using namespace webgpu;
  wgpu::InstanceDescriptor id{};
  const wgpu::InstanceFeatureName timed=wgpu::InstanceFeatureName::TimedWaitAny;
  id.requiredFeatureCount=1;id.requiredFeatures=&timed;
  auto instance=wgpu::CreateInstance(&id);
  wgpu::Adapter adapter;
  wgpu::RequestAdapterOptions options{.backendType=wgpu::BackendType::D3D12};
  auto future=instance.RequestAdapter(&options,wgpu::CallbackMode::WaitAnyOnly,
    [&](wgpu::RequestAdapterStatus status,wgpu::Adapter a,wgpu::StringView message) {
      if(status==wgpu::RequestAdapterStatus::Success) adapter=std::move(a);
      else std::cerr<<std::string_view(message)<<'\n';
    });
  if(instance.WaitAny(future,5000000000)!=wgpu::WaitStatus::Success||!adapter) return 1;
  wgpu::DeviceDescriptor dd{};
  dd.SetUncapturedErrorCallback([](const wgpu::Device&,wgpu::ErrorType,wgpu::StringView message) {
    ++errors;std::cerr<<std::string_view(message)<<'\n';
  });
  future=adapter.RequestDevice(&dd,wgpu::CallbackMode::WaitAnyOnly,
    [&](wgpu::RequestDeviceStatus status,wgpu::Device device,wgpu::StringView message) {
      if(status==wgpu::RequestDeviceStatus::Success) g_device=std::move(device);
      else std::cerr<<std::string_view(message)<<'\n';
    });
  if(instance.WaitAny(future,5000000000)!=wgpu::WaitStatus::Success||!g_device) return 1;
  g_graphicsConfig.surfaceConfiguration.format=wgpu::TextureFormat::RGBA8Unorm;
  g_graphicsConfig.depthFormat=wgpu::TextureFormat::Depth32Float;
  for(uint32_t samples : {1u,4u}) {
    gfx::StereoReplayFrame frame{};
    frame.cockpit.active=true;frame.cockpit.wheelAngle=0.35f;
    for(int hand=0;hand<2;++hand) {
      auto& h=frame.cockpit.hands[hand];h.tracked=true;h.held=true;h.squeeze=1;
      auto pose=gfx::cockpit::identity();pose[3]=hand?0.18f:-0.18f;pose[7]=-0.30f;pose[11]=-0.42f;
      std::memcpy(h.seatFromGrip,pose.data(),sizeof(h.seatFromGrip));
    }
    wgpu::TextureDescriptor td{.usage=wgpu::TextureUsage::RenderAttachment|wgpu::TextureUsage::CopySrc,
      .size={512,512,1},.format=wgpu::TextureFormat::RGBA8Unorm,.sampleCount=1};
    auto output=g_device.CreateTexture(&td);
    td.sampleCount=samples;td.usage=wgpu::TextureUsage::RenderAttachment;
    auto color=g_device.CreateTexture(&td);
    td.format=wgpu::TextureFormat::Depth32Float;auto depth=g_device.CreateTexture(&td);
    auto& eye=frame.eyes[0];eye.target.colorView=samples==1?output.CreateView():color.CreateView();
    if(samples>1) eye.target.resolveView=output.CreateView();
    eye.target.depthView=depth.CreateView();eye.target.size={512,512,1};eye.target.msaaSamples=samples;
    eye.projection.m0[0]=1;eye.projection.m1[1]=1;
    auto view=gfx::cockpit::identity();view[7]=0.20f;
    std::memcpy(frame.cockpit.eyeFromSeat[0],view.data(),sizeof(frame.cockpit.eyeFromSeat[0]));
    auto encoder=g_device.CreateCommandEncoder();
    const wgpu::RenderPassColorAttachment clear{.view=eye.target.colorView,.resolveTarget=eye.target.resolveView,
      .loadOp=wgpu::LoadOp::Clear,.storeOp=wgpu::StoreOp::Store,.clearValue={0.06,0.09,0.13,1}};
    const wgpu::RenderPassDescriptor pd{.colorAttachmentCount=1,.colorAttachments=&clear};
    auto pass=encoder.BeginRenderPass(&pd);pass.End();
    gfx::cockpit::render(encoder,frame,0);
    const wgpu::BufferDescriptor bd{.usage=wgpu::BufferUsage::CopyDst|wgpu::BufferUsage::MapRead,.size=512*512*4};
    auto readback=g_device.CreateBuffer(&bd);
    const wgpu::TexelCopyTextureInfo src{.texture=output};
    const wgpu::TexelCopyBufferInfo dst{.layout={.bytesPerRow=2048,.rowsPerImage=512},.buffer=readback};
    const wgpu::Extent3D extent{512,512,1};encoder.CopyTextureToBuffer(&src,&dst,&extent);
    auto commands=encoder.Finish();g_device.GetQueue().Submit(1,&commands);
    bool mapped=false;
    future=readback.MapAsync(wgpu::MapMode::Read,0,512*512*4,wgpu::CallbackMode::WaitAnyOnly,
      [&](wgpu::MapAsyncStatus status,wgpu::StringView) { mapped=status==wgpu::MapAsyncStatus::Success; });
    if(instance.WaitAny(future,5000000000)!=wgpu::WaitStatus::Success||!mapped) return 1;
    const auto* bytes=static_cast<const unsigned char*>(readback.GetConstMappedRange());
    size_t bright=0;
    for(size_t i=0;i<512*512;++i) if(bytes[4*i]>90&&bytes[4*i+1]>90&&bytes[4*i+2]>90) ++bright;
    if(bright<1000) { std::cerr<<"Hands/wheel did not reach color target\n";++errors; }
    if(samples==4) {
      std::ofstream image("cockpit-preview.ppm",std::ios::binary);image<<"P6\n512 512\n255\n";
      for(size_t i=0;i<512*512;++i) image.write(reinterpret_cast<const char*>(bytes+i*4),3);
    }
    readback.Unmap();
    std::cout<<samples<<"x MSAA: "<<bright<<" visible geometry pixels\n";
  }
  gfx::cockpit::shutdown();g_device.Destroy();g_device=nullptr;
  return errors?1:0;
}
