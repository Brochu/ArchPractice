#include <stdio.h>
#include <string.h>
#include <vector>

void Debug(const std::vector<int>& nums)
{
    for(const int n : nums)
    {
        printf("%i ", n);
    }
    printf("\n");
}

// Quicksort START
int HoarePartition(int *arr, int lo, int hi)
{
    return -1;
}

void Quicksort(int *arr, int lo, int hi)
{
}
// Quicksort END


// Merge sort START
// Merge sort END
int main(int argc, char **argv)
{
    std::vector<int> nums { 65, 23, 10, 89, 99, 72, 52, 12, 45, 0, 16, 79, 81 };

    printf("[MAIN] Welcome to sorting\n");
    printf("[MAIN] Sorting the following list\n");
    Debug(nums);

    if (argc == 2)
    {
        if (strcmp(argv[1], "Quick") == 0)
        {
            Quicksort(nums.data(), 0, nums.size() - 1);
        }
        else if (strcmp(argv[1], "Merge") == 0)
        {
            //TODO: Implement merge sort
        }
    }
    else
    {
        printf("[MAIN] Please only specify sort type (Quick, Merge)\n");
    }

    printf("[MAIN] ... DONE ...\n");
    printf("[MAIN] Here is the sorted list\n");
    Debug(nums);

    return 0;
}
