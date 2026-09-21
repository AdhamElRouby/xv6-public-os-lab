#include "types.h"
#include "user.h"
#include "math.h"

int main(int argc, char *argv[])
{
    if(argc < 2) {
        printf(1, "Usage: %s num1 num2 ... numN\n", argv[0]);
        exit();
    }
    int size = argc - 1;
    double* arr = malloc(sizeof(double) * (size));
    if(arr == 0) {
        printf(1, "sort: memory allocation failed\n");
        exit();
    }
    for(int i = 1; i < argc; i++)
        arr[i - 1] = atof(argv[i]);

    mergeSort(arr, 0, size - 1);
    for(int i = 0; i < size; i++)
        printf(1, "%f ", arr[i]);
    printf(1, "\n");
    free(arr);
    exit();
}