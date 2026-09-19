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
        printf(1, "printstats: memory allocation failed\n");
        exit();
    }
    for(int i = 1; i < argc; i++)
        arr[i - 1] = atof(argv[i]);

    double min_value = min(arr, size);
    double max_value = max(arr, size);
    double median_value = median(arr, size);
    double avg = average(arr, size);
    double std = stddev(arr, size);

    printf(1, "Min: %f\n", min_value);
    printf(1, "Max: %f\n", max_value);
    printf(1, "Average: %f\n", avg);
    printf(1, "Median: %f\n", median_value);
    printf(1, "Standard Deviation: %f\n", std);

    free(arr);
    exit();
}