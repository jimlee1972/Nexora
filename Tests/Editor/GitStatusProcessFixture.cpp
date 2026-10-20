#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>

int main(int argc, char **argv) {
  if (argc < 1)
    return 2;
  const auto name = std::filesystem::path(argv[0]).filename().string();
  if (name.find("overflow") != std::string::npos)
    std::cout << std::string(300 * 1024, 'x');
  else if (name.find("sleep") != std::string::npos) {
    std::ofstream(".fixture-ready") << "ready";
    for (;;)
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
  } else
    std::cout << " M missing NUL";
  return 0;
}
