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
size_t HoarePartition(int *arr, int lo, int hi)
{
    int pivot = arr[(lo + hi) / 2];

    int i = lo, j = hi;

    while(true)
    {
        while(arr[i] < pivot) i++;
        while(arr[j] > pivot) j--;

        if (i >= j) return j;

        std::swap(arr[i], arr[j]);
    }

    return -1;
}

void Quicksort(int *arr, int lo, int hi)
{
    // Check base case for recursion
    if (lo >= hi)
        return;

    size_t p = HoarePartition(arr, lo, hi);

    Quicksort(arr, lo, p);
    Quicksort(arr, p + 1, hi);
}
// Quicksort END
void Mergesort(int *arr, int lo, int hi, int *sorted)
{
    if (lo >= hi)
        return;

    int mid = (lo + hi) / 2;
    printf("[MERGE] middle index found = %i\n", mid);

    Mergesort(arr, lo, mid, sorted);
    Mergesort(arr, mid + 1, hi, sorted);
}
// Merge sort START

// Merge sort END
int main(int argc, char **argv)
{
    if (argc != 2)
    {
        printf("[MAIN] Please only specify sort type (Quick, Merge)\n");
        return 0;
    }

    std::vector<int> nums { 65, 23, 10, 89, 99, 72, 52, 12, 45, 0, 16, 79, 81 };
    std::vector<int> sorted(nums.size());

    printf("[MAIN] Welcome to sorting\n");
    printf("[MAIN] Sorting the following list\n");
    Debug(nums);

    if (strcmp(argv[1], "Quick") == 0)
    {
        Quicksort(nums.data(), 0, nums.size() - 1);
    }
    else if (strcmp(argv[1], "Merge") == 0)
    {
        Mergesort(nums.data(), 0, nums.size() - 1, sorted.data());
    }
    else
    {
        printf("[MAIN] Sorting algo not supported\n");
        return 0;
    }

    printf("[MAIN] ... DONE ...\n");
    printf("[MAIN] Here is the sorted list\n");
    Debug(nums);

    return 0;
}
