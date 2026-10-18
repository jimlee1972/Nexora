from pathlib import Path
p = Path(__file__).with_name('BenchmarkProfile.cpp')
s = p.read_text()
needle = '  std::vector<double> cpu, gpu, whole;'
assert s.count(needle)==1
s = s.replace(needle, '''  if (std::getenv("NEXORA_TRACE_RENDER_ONLY")) {
    std::vector<twod::SpriteInstance> sprites;
    std::uint64_t id=0;
    pool.ForEach([&](auto &instance) { sprites.push_back({++id,instance.sprite,runtime::ToMatrix(instance.transform)}); });
    auto fixed = twod::ExtractSprites(sprites,camera,[&](auto)->std::optional<twod::AtlasBinding>{return twod::AtlasBinding{shared,201};});
    Check(bool(fixed),"render-only extraction");
    std::vector<std::array<double,9>> trace; trace.reserve(frames+4);
    for (unsigned frame=0; frame<frames+4; ++frame) {
      ProfileTrace::Begin(); auto start=Clock::now();
      Check(surface.surface->BeginFrame()==Nexora::Presentation::SurfaceStatus::Ready,"render-only acquire");
      auto acquired=Clock::now();
      Check(fixed->Draw(*surface.surface)==Nexora::Presentation::SurfaceStatus::Ready,"render-only draw");
      auto drawn=Clock::now();
      Check(surface.surface->EndFrame()==Nexora::Presentation::SurfaceStatus::Ready,"render-only present");
      auto end=Clock::now(); auto n=ProfileTrace::Current();
      const auto ms=[](auto a,auto b){return std::chrono::duration<double,std::milli>(b-a).count();};
      trace.push_back({ms(start,acquired),ms(acquired,drawn),ms(drawn,end),n.fence,n.acquire,n.submit,n.present,double(n.images),double(n.mode)});
    }
    Check(surface.surface->DrainAndDestroy()==Nexora::Presentation::SurfaceStatus::Ready,"render-only drain");
    for(unsigned i=0;i<trace.size();++i){std::cerr<<"RENDER,"<<count<<','<<sparse<<','<<overlap<<','<<i;for(auto value:trace[i])std::cerr<<','<<value;std::cerr<<'\\n';}
    return;
  }
''' + needle)
p.write_text(s)
