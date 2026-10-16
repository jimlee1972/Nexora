#include "TwoDPlayer.h"
extern "C" int32_t NexoraGameModuleLoad(uint32_t, NexoraGameModuleV3 *);
#if defined(_WIN32)
int wmain(int argc, wchar_t **argv) {
#else
int main(int argc, char **argv) {
#endif
  return nexora::player::DispatchTwoDPlayer(argc, argv, NexoraGameModuleLoad);
}
