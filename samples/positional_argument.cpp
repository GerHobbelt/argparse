// SPDX-License-Identifier: MIT

#include <argparse/argparse.hpp>


#if defined(BUILD_MONOLITHIC)
#define main    argparse_positional_arguments_main
#endif

int main(int argc, const char **argv) {
  argparse::ArgumentParser program("main");

  program.add_argument("square")
      .help("display the square of a given number")
      .scan<'i', int>();

  program.add_argument("--verbose").flag();

  try {
    program.parse_args(argc, argv);
  } catch (const std::exception &err) {
    std::cerr << err.what() << std::endl;
    std::cerr << program;
    return 1;
  }

  int input = program.get<int>("square");

  if (program["--verbose"] == true) {
    std::cout << "The square of " << input << " is " << (input * input)
              << std::endl;
  } else {
    std::cout << (input * input) << std::endl;
  }

  return 0;
}
