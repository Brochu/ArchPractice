#include <stdio.h>
#include <vector>

void Quicksort(int *arr, int s, int e)
{
    printf("[Quicksort] before work: \n");
    for (int i = s; i < e; i++)
    {
        printf("%i, ", arr[i]);
    }
    printf("\n");
}

int main(int argc, char **argv)
{
    printf("[MAIN] Quicksort: \n");
    std::vector<int> array = { 34, 59, 12, 43, 90, 83, 72, 86, 36, 29, 21, 0 };
    Quicksort(array.data(), 0, array.size());

    return 0;
}
