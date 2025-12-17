#include <stdio.h>

__global__ void test() {
    printf("CUDA OK from GPU thread %d\n", threadIdx.x);
}

int main() {
    test<<<1, 5>>>();
    cudaDeviceSynchronize();
}

