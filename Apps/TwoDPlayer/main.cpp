#include "TwoDPlayer.h"
#if defined(_WIN32)
int wmain(int argc, wchar_t **argv) { return nexora::player::DispatchTwoDPlayer(argc, argv); }
#else
int main(int argc, char **argv) { return nexora::player::DispatchTwoDPlayer(argc, argv); }
#endif
