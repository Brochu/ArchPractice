#include <stdio.h>
#include <vector>

void Debug(int *arr, int size)
{
    printf("Array: ");
    for (int i = 0; i < size; i++)
    {
        printf("%i ", arr[i]);
    }
    printf("\n");
}

int Partition(int *arr, int l, int h)
{
    int pivot = arr[(h + l) / 2];

    int i = l - 1, j = h + 1;

    while(true)
    {
        do i++; while(arr[i] < pivot);
        do j--; while(arr[j] > pivot);

        if (i >= j) return j;

        std::swap(arr[i], arr[j]);
    }

    return -1;
}

void Quicksort(int *arr, int l, int h)
{
    // Base case, we can stop recursion here
    if (l >= h) return;

    int p = Partition(arr, l, h);

    Quicksort(arr, l, p);
    Quicksort(arr, p + 1, h);
}

int main(int argc, char **argv)
{
    std::vector<int> array { 32, 56, 78, 92, 12, 18, 0, 23, 11, 45, 63 };

    printf("[MAIN] Before quicksort\n");
    Debug(array.data(), array.size());
    Quicksort(array.data(), 0, array.size() - 1);

    printf("[MAIN] After quicksort\n");
    Debug(array.data(), array.size());

    return 0;
}
