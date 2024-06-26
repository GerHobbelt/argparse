// SPDX-License-Identifier: MIT

#include <argparse/argparse.hpp>


#if defined(BUILD_MONOLITHIC)
#define main    argparse_description_epilog_metavar_main
#endif

int main(int argc, const char **argv) {
  argparse::ArgumentParser program("main");
  program.add_argument("thing").help("Thing to use.").metavar("THING");
  program.add_argument("--member")
      .help("The alias for the member to pass to.")
      .metavar("ALIAS");
  program.add_argument("--verbose").flag();

  program.add_description("Forward a thing to the next member.");
  program.add_epilog("Possible things include betingalw, chiz, and res.");

  program.parse_args(argc, argv);

  std::cout << program << std::endl;

  return 0;
}
