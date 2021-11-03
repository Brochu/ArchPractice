#include <memory>
#include <stdio.h>
#include <string>

class MyManager
{
    public:
        static MyManager& GetInstance()
        {
            static std::unique_ptr<MyManager> instance(new MyManager());

            return *instance;
        }

        std::string GetValue() const { return _val; }
        void SetValue(std::string& newVal) { _val = std::string(newVal); }

    private:
        MyManager(){};

        MyManager(MyManager&) = delete;
        void operator=(MyManager&) = delete;

        std::string _val;
};

int main(int argc, char** argv)
{
    printf("[MAIN] First call to `GetInstance` will create the instance.\n");
    printf("[MAIN] Current value in the Manager = %s.\n", MyManager::GetInstance().GetValue().c_str());

    std::string newVal = "Hello, world!!";
    MyManager::GetInstance().SetValue(newVal);
    printf("[MAIN] New value in the Manager = %s.\n", MyManager::GetInstance().GetValue().c_str());

    return 0;
}
