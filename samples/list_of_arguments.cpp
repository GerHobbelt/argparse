// SPDX-License-Identifier: MIT

#include <argparse/argparse.hpp>


#if defined(BUILD_MONOLITHIC)
#define main    argparse_list_of_arguments_main
#endif

int main(int argc, const char **argv) {

  argparse::ArgumentParser program("main");

  program.add_argument("--input_files")
      .help("The list of input files")
      .nargs(2);

  try {
    program.parse_args(
        argc, argv); // Example: ./main --input_files config.yml System.xml
  } catch (const std::exception &err) {
    std::cerr << err.what() << std::endl;
    std::cerr << program;
    return 1;
  }

  auto files = program.get<std::vector<std::string>>(
      "--input_files"); // {"config.yml", "System.xml"}

  if (!files.empty()) {
    std::cout << "Files: ";
    for (auto &file : files) {
      std::cout << file << " ";
    }
    std::cout << std::endl;
  }

  return 0;
}
