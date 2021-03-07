#include "session.h"

#include <iostream>
#include <stdexcept>

int main(int argc, char* argv[]) try {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " <username> <password>\n";
    return 1;
  }

  std::string const username = argv[1];
  std::string const password = argv[2];

  tocata::Session session{};

  session.connect(username, password);

  std::cout << "Connection closed" << "\n";

  return 0;
} catch (std::exception const& ex) {
  std::cerr << "Standard exception raised: " << ex.what() << "\n";
  return 1;
}
