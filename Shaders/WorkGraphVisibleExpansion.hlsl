// Optional SM6.8 node library; compiled directly by SDK DXC, independently from Slang.
// Five-word visible record ABI matches the existing GPUDriven.slang compute fallback.
RWStructuredBuffer<uint> graphOutput : register(u1);
struct InputRecord {
  uint count;
  uint firstObjectSlot;
#if defined(NEXORA_WORK_GRAPH_WRONG_ABI)
  uint extra;
#endif
};
struct ChildRecord { uint index; uint objectSlot; };
[Shader("node")]
[NodeLaunch("broadcasting")]
[NodeIsProgramEntry]
[NodeDispatchGrid(1, 1, 1)]
[NumThreads(1, 1, 1)]
void producer(DispatchNodeInputRecord<InputRecord> input,
              [NodeID("consumer")] [MaxRecords(8)] NodeOutput<ChildRecord> child) {
  GroupNodeOutputRecords<ChildRecord> records = child.GetGroupNodeOutputRecords(input.Get().count);
  for (uint i = 0; i < input.Get().count; ++i) {
    records[i].index = i;
    records[i].objectSlot = input.Get().firstObjectSlot + i;
#if defined(NEXORA_WORK_GRAPH_WRONG_ABI)
    records[i].objectSlot += input.Get().extra;
#endif
  }
  records.OutputComplete();
}
[Shader("node")]
[NodeLaunch("thread")]
void consumer(ThreadNodeInputRecord<ChildRecord> input) {
  uint base = input.Get().index * 5;
  graphOutput[base] = input.Get().objectSlot;
  graphOutput[base + 1] = 1;
  graphOutput[base + 2] = 11;
  graphOutput[base + 3] = 13;
  graphOutput[base + 4] = 0;
}
