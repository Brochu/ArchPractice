#include <stdio.h>
#include <vector>

int HoarePartition(char *arr, int lo, int hi)
{
    const char pivot = arr[(lo + hi) / 2];

    int i = lo, j = hi;

    while(true)
    {
        while(arr[i] < pivot) i++;
        while(arr[j] > pivot) j--;

        if (i >= j)
            return j;

        std::swap(arr[i], arr[j]);
    }

    return -1;
}

void Quicksort(char *arr, int lo, int hi)
{
    if (lo >= hi)
        return;

    const int p = HoarePartition(arr, lo, hi);

    Quicksort(arr, lo, p);
    Quicksort(arr, p + 1, hi);
}

int main(int argc, char **argv)
{
    printf("[MAIN] Sorting data using Quicksort\n");

    std::vector<char> nums { 6, 22, 14, 78, 99, 30, 44, 59, 21, 17 };
    printf("[MAIN] List before sorting : \n");
    for (const char& n : nums)
    {
        printf(" %d ", n);
    }
    printf("\n\n");

    Quicksort(nums.data(), 0, nums.size() - 1);
    printf("[MAIN] List after sorting : \n");
    for (const char& n : nums)
    {
        printf(" %d ", n);
    }
    printf("\n\n");

    return 0;
}
