
#include <iostream>
#include <fstream>
#include <filesystem>
#include "elysiumx/parser.hpp"

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <input_file> <output_file>" << std::endl;
        return 1;
    }

    std::string inputFile = argv[1];
    std::string outputFile = argv[2];

    try {
        std::filesystem::path inputPath(inputFile);
        elysiumx::Parser parser(inputPath.parent_path().string());
        std::string result = parser.parseFile(inputFile);

        std::ofstream outFile(outputFile);
        outFile << result;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}

