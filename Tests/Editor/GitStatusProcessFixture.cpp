#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#if defined(_WIN32)
#include <cstdio>
#include <fcntl.h>
#include <io.h>
#endif

int main(int argc, char **argv) {
  if (argc < 1)
    return 2;
#if defined(_WIN32)
  if (_setmode(_fileno(stdout), _O_BINARY) == -1)
    return 2;
#endif
  const auto name = std::filesystem::path(argv[0]).filename().string();
  if (name.find("maximum") != std::string::npos)
    std::cout << std::string(256 * 1024, 'x');
  else if (name.find("raw") != std::string::npos)
    std::cout.write("warning\nraw\0bytes", 17);
  else if (name.find("overflow") != std::string::npos)
    std::cout << std::string(300 * 1024, 'x');
  else if (name.find("sleep") != std::string::npos) {
    std::ofstream(".fixture-ready") << "ready";
    for (;;)
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
  } else
    std::cout << " M missing NUL";
  return 0;
}
