// Intentionally valid in both C/C++ and Slang. This is the single source of truth for the packed
// GPU-driven indirect-command word offsets consumed by every native backend and shader target.
#define NEXORA_INDIRECT_VERTEX_COUNT_WORD 0
#define NEXORA_INDIRECT_INSTANCE_COUNT_WORD 1
#define NEXORA_INDIRECT_FIRST_VERTEX_WORD 2
#define NEXORA_INDIRECT_FIRST_INSTANCE_WORD 3
#define NEXORA_INDIRECT_MESH_WORD 4
#define NEXORA_INDIRECT_MATERIAL_WORD 5
#define NEXORA_INDIRECT_LOD_WORD 6
#define NEXORA_INDIRECT_COMPACTED_FIRST_INSTANCE_WORD 7
#define NEXORA_INDIRECT_COMPACTED_INSTANCE_COUNT_WORD 8
#define NEXORA_GPU_DRIVEN_INDIRECT_COMMAND_WORDS 9
