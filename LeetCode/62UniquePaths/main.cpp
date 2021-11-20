#include <stdio.h>
#include <stdlib.h>
#include <map>

std::map<long, std::map<long, long>> cache;

long uniquePathsImpl(long cm, long cn)
{
    if (cache[cm][cn] != 0) return cache[cm][cn];
    if (cm == 0 && cn == 0) return 1;

    if (cm == 0)
    {
        long partial = uniquePathsImpl(cm, cn - 1);
        cache[cm][cn] = partial;
        return partial;
    }
    if (cn == 0)
    {
        long partial = uniquePathsImpl(cm - 1, cn);
        cache[cm][cn] = partial;
        return partial;
    }

    long n0 = uniquePathsImpl(cm - 1, cn);
    long n1 = uniquePathsImpl(cm, cn - 1);

    cache[cm][cn] = n0 + n1;
    return n0 + n1;
}

long uniquePaths(long m, long n)
{
    return uniquePathsImpl(m - 1, n - 1);
}

int main(int argc, char** argv)
{
    if (argc == 3)
    {
        long result = uniquePaths(atoi(argv[1]), atoi(argv[2]));

        printf("Unique Paths; result = %li\n", result);
        return 0;
    }
    else
    {
        printf("Nope\n");
        return -1;
    }
}
