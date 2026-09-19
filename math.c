#include "types.h"
#include "user.h"
#include "math.h"

double average(double arr[], int size) {
    double sum = 0;
    // Compute the arithmetic mean without changing the input array.
    for(int i = 0; i < size; i++) {
        sum += arr[i];
    }
    return sum / size;
}

double sqrt(double n) {
    if(n < 0) {
        printf(1, "sqrt: negative input %f is not allowed\n", n);
        exit();
    }
    // Use the Newton-Raphson method to approximate the square root
    double guess = n / 2.0;
    double epsilon = 0.000001;

    while (guess * guess - n > epsilon || n - guess * guess > epsilon)
        guess = (guess + n / guess) / 2;

    return guess;
}

double stddev(double arr[], int size) {
    double avg = average(arr, size);
    double sum = 0;
    // This is population standard deviation: divide by size, not size - 1.
    for(int i = 0; i < size; i++) {
        sum += (arr[i] - avg) * (arr[i] - avg);
    }
    return sqrt(sum / size);
}

double min(double arr[], int size) {
    double min = arr[0];
    for(int i = 1; i < size; i++) {
        if(arr[i] < min) {
            min = arr[i];
        }
    }
    return min;
}

double max(double arr[], int size) {
    double max = arr[0];
    for(int i = 1; i < size; i++) {
        if(arr[i] > max) {
            max = arr[i];
        }
    }
    return max;
}

double median(double arr[], int size) {
    // Sort a copy so finding the median preserves the caller's array.
    double* arr_copy = malloc(sizeof(double) * size);
    if(arr_copy == 0) {
        printf(1, "printstats: memory allocation failed\n");
        exit();
    }
    for(int i = 0; i < size; i++)
        arr_copy[i] = arr[i];
    mergeSort(arr_copy, 0, size - 1);

    double result;
    if(size % 2 == 0) {
        // Even-sized inputs use the mean of the two middle values.
        result = (arr_copy[size / 2 - 1] + arr_copy[size / 2]) / 2.0;
    } else {
        result = arr_copy[size / 2];
    }
    free(arr_copy);
    return result;
}

static void merge(double arr[], int l, int m, int r)
{
    int n1 = m - l + 1;
    int n2 = r - m;
    double L[n1], R[n2];

    for(int i = 0; i < n1; i++)
        L[i] = arr[l + i];
    for(int i = 0; i < n2; i++)
        R[i] = arr[m + 1 + i];

    int i = 0, j = 0, k = l;
    while(i < n1 && j < n2) {
        if(L[i] <= R[j])
            arr[k++] = L[i++];
        else
            arr[k++] = R[j++];
    }
    while(i < n1)
        arr[k++] = L[i++];
    while(j < n2)
        arr[k++] = R[j++];
}

void mergeSort(double arr[], int l, int r)
{
    if(l < r) {
        // Recursively split the range, then merge the sorted halves.
        int m = l + (r - l) / 2;
        mergeSort(arr, l, m);
        mergeSort(arr, m + 1, r);
        merge(arr, l, m, r);
    }
}