// SPDX-License-Identifier: MIT

#include <argparse/argparse.hpp>


#if defined(BUILD_MONOLITHIC)
#define main    argparse_optional_flag_arguments_main
#endif

int main(int argc, const char **argv) {
  argparse::ArgumentParser program("test");

  program.add_argument("--verbose")
      .help("increase output verbosity")
      .default_value(false)
      .implicit_value(true);

  try {
    program.parse_args(argc, argv);
  } catch (const std::exception &err) {
    std::cerr << err.what() << std::endl;
    std::cerr << program;
    return 1;
  }

  if (program["--verbose"] == true) {
    std::cout << "Verbosity enabled" << std::endl;
  }

  return 0;
}
