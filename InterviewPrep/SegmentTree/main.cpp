#include <stdio.h>
#include <algorithm>
#include <string.h>

void Print(int *arr, int n)
{
    printf("====\n");
    for (int i = 0; i < n; i++)
    {
        printf("%i ", arr[i]);
    }
    printf("\n====\n");
}

void BuildSegmentTree(int *arr, int n)
{
    for (int i = n - 1; i > 0; i--)
    {
        arr[i] = arr[2*i] + arr[2*i+1];
    }
}

int QuerySegmentTree(int *arr, int n, int l, int r)
{
    int res = 0;

    for (l += n, r += n; l < r; l >>= 1, r >>= 1)
    {
        if (l&1) res += arr[l++];
        if (r&1) res += arr[--r];
    }
    return res;
}

int main(int argc, char **argv)
{
    int values[] = { 58, 62, 15, 92, 17, 80, 95, 0 };
    int n = sizeof(values) / sizeof(values[0]);

    int sTree[2 * n];
    int m = sizeof(sTree) / sizeof(sTree[0]);
    memset(sTree, 0, m * sizeof(sTree[0]));
    memcpy(sTree + n, values, n * sizeof(values[0]));

    printf("[MAIN] Segment tree tests ...\n");

    printf("[MAIN] Main values (n = %i):\n", n);
    Print(values, n);

    printf("[MAIN] Segment tree initial state (m = %i):\n", m);
    Print(sTree, m);

    BuildSegmentTree(sTree, n);

    printf("[MAIN] Segment tree done building:\n");
    Print(sTree, m);

    int l = 2, r = 6;
    printf("[MAIN] Query from %i to %i : %i\n", l, r, QuerySegmentTree(sTree, n, l, r));

    return 0;
}
