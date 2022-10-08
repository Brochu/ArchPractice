#include <algorithm>
#include <assert.h>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string.h>
#include <vector>
#include <limits>

int taille_ = 0;
int* entiers_ = nullptr;

void supprimeEntiers(const std::vector<int>& index)
{
    for (int i = 0; i < index.size(); i++) {
        assert(0 <= index[i] && index[i] < taille_);
    }

    std::vector<int> local = index;
    std::sort(local.begin(), local.end(), std::greater<int>());

    for (int i = 0; i < local.size(); i++) {
        int rem = local[i];
        std::memcpy(&entiers_[rem], &entiers_[rem+1], (taille_ - rem+1) * sizeof(int));
    }
    taille_ -= local.size();
}

int main(int argc, char** argv) {
    taille_ = 10;
    entiers_ = new int[10];
    entiers_[0] = 0;
    entiers_[1] = 1;
    entiers_[2] = 2;
    entiers_[3] = 3;
    entiers_[4] = 4;
    entiers_[5] = 5;
    entiers_[6] = 6;
    entiers_[7] = 7;
    entiers_[8] = 8;
    entiers_[9] = 9;

    for(int i = 0; i < taille_; i++) {
        printf("%i, ", entiers_[i]);
    }
    printf("\n");

    printf("Calling supprimeEntiers!\n");
    supprimeEntiers({7, 3, 5, 3});

    for(int i = 0; i < taille_; i++) {
        printf("%i, ", entiers_[i]);
    }
    printf("\n");
    printf("DONE!\n");

    return 0;
}
