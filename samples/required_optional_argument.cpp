// SPDX-License-Identifier: MIT

#include <argparse/argparse.hpp>


#if defined(BUILD_MONOLITHIC)
#define main    argparse_required_optional_arguments_main
#endif

int main(int argc, const char **argv) {
  argparse::ArgumentParser program("test");

  program.add_argument("-o", "--output")
      .required()
      .help("specify the output file.");

  try {
    program.parse_args(argc, argv);
  } catch (const std::runtime_error &err) {
    std::cerr << err.what() << std::endl;
    std::cerr << program;
    return 1;
  }

  std::cout << "Output written to " << program.get("-o") << "\n";

  return 0;
}
