// SPDX-License-Identifier: MIT

#include <argparse/argparse.hpp>


#if defined(BUILD_MONOLITHIC)
#define main    argparse_is_used_main
#endif

int main(int argc, const char **argv) {
  argparse::ArgumentParser program("test");

  program.add_argument("--color")
      .default_value(std::string{
          "orange"}) // might otherwise be type const char* leading to an error
                     // when trying program.get<std::string>
      .help("specify the cat's fur color");

  try {
    program.parse_args(argc, argv); // Example: ./main --color orange
  } catch (const std::exception &err) {
    std::cerr << err.what() << std::endl;
    std::cerr << program;
    return 1;
  }

  auto color = program.get<std::string>("--color"); // "orange"
  auto explicit_color =
      program.is_used("--color"); // true, user provided orange
  std::cout << "Color: " << color << "\n";
  std::cout << "Argument was explicitly provided by user? " << std::boolalpha
            << explicit_color << "\n";

  return 0;
}
