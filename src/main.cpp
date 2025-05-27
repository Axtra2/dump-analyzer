#include <app/app.h>

#include <cstdlib>
#include <iostream>
#include <stdexcept>

int main(int argc, char* argv[]) try {
    App app;
    app.run(parseArgs(argc, argv));

    // destructors take a very long time for some reason,
    // so exit without running destructors to save time
    std::exit(EXIT_SUCCESS);
} catch (const std::exception& e) {
    std::cout << "error: " << e.what() << std::endl;
}
