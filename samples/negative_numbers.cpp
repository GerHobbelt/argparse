// SPDX-License-Identifier: MIT

#include <argparse/argparse.hpp>


#if defined(BUILD_MONOLITHIC)
#define main    argparse_negative_numbers_main
#endif

int main(int argc, const char **argv) {
  argparse::ArgumentParser program("test");

  program.add_argument("integer").help("Input number").scan<'i', int>();

  program.add_argument("floats")
      .help("Vector of floats")
      .nargs(4)
      .scan<'g', float>();

  try {
    program.parse_args(argc, argv);
  } catch (const std::exception &err) {
    std::cerr << err.what() << std::endl;
    std::cerr << program;
    return 1;
  }

  if (program.is_used("integer")) {
    std::cout << "Integer : " << program.get<int>("integer") << "\n";
  }

  if (program.is_used("floats")) {
    std::cout << "Floats  : ";
    for (const auto &f : program.get<std::vector<float>>("floats")) {
      std::cout << f << " ";
    }
    std::cout << std::endl;
  }

  return 0;
}
