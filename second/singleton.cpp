#include <stdio.h>
#include <string>

class MyManager
{
    public:
        std::string value;

    private:
        MyManager(){};
        ~MyManager(){};

        MyManager(MyManager&) = delete;
        void operator=(MyManager&) = delete;
};

int main(int argc, char** argv)
{
    return 0;
}
