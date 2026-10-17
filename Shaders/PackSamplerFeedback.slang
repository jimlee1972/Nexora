Texture2D<uint> decoded : register(t0);
RWStructuredBuffer<uint> packed : register(u1);
[numthreads(1,1,1)]
void packFeedback(uint3 id:SV_DispatchThreadID) {
 if(id.x<16) packed[id.x]=decoded.Load(int3(id.x%4,id.x/4,0));
}
