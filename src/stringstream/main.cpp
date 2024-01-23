#include<sstream>
#include<iostream>

int main(int argc, char **argv) {
    std::istringstream double_stream;
    double_stream.str("69.86");

    double extract = 0.0d;

    std::cout << extract << std::endl;

    std::cout << double_stream.get() << std::endl;

    double_stream >> extract;

    std::cout << extract << std::endl;
    return 0;
}