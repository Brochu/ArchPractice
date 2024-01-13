#include <stdio.h>

#include <string>
#include <fstream>
#include <sstream>

int main(int argc, char** argv)
{
    printf("Starting Test ...\n");

    const char* values = "vn 16.6 14.4 15.5";
    std::stringstream ss(values);

    std::string type;
    ss >> type;

    float f0, f1, f2;
    ss >> f0;
    ss >> f1;
    ss >> f2;
    printf("Parsed:\n%s:\n%f\n%f\n%f\n", type.c_str(), f0, f1, f2);

    printf("Test Over ...\n");
    return 0;
}
