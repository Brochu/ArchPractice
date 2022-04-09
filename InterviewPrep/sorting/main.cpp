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
void Merge(int *arr, int lo, int mid, int hi)
{
    int lLen = mid - lo + 1, rLen = hi - mid;

    int left[lLen];
    for (int i = 0; i < lLen; i++)
        left[i] = arr[lo + i];

    int right[rLen];
    for (int j = 0; j < rLen; j++)
        right[j] = arr[mid + 1 + j];

    int i = 0, j = 0, k = lo;

    while(i < lLen && j < rLen)
    {
        if (left[i] <= right[j])
            arr[k++] = left[i++];
        else
            arr[k++] = right[j++];
    }

    while(i < lLen)
        arr[k++] = left[i++];

    while(j < rLen)
        arr[k++] = right[j++];
}

void Mergesort(int *arr, int lo, int hi)
{
    if (lo >= hi)
        return;

    int mid = (lo + hi) / 2;

    Mergesort(arr, lo, mid);
    Mergesort(arr, mid + 1, hi);
    Merge(arr, lo, mid, hi);
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

    printf("[MAIN] Welcome to sorting\n");
    printf("[MAIN] Sorting the following list\n");
    Debug(nums);

    if (strcmp(argv[1], "Quick") == 0)
    {
        Quicksort(nums.data(), 0, nums.size() - 1);
        printf("[MAIN] ... DONE ...\n");
        printf("[MAIN] Here is the sorted list\n");
        Debug(nums);
    }
    else if (strcmp(argv[1], "Merge") == 0)
    {
        Mergesort(nums.data(), 0, nums.size() - 1);
        printf("[MAIN] ... DONE ...\n");
        printf("[MAIN] Here is the sorted list\n");
        Debug(nums);
    }
    else
    {
        printf("[MAIN] Sorting algo not supported\n");
        return 0;
    }

    return 0;
}
