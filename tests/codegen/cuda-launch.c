void go(int n, cudaStream_t stream, void (*k)(int)) {
add<<<blocks, threads>>>(n, 2.0f, d_x, d_y);
add<<<dim3(16, 16), dim3(8, 8)>>>(n);
reduce<<<grid, block, (n * sizeof(float))>>>(d_in, d_out);
reduce<<<grid, block, 0, stream>>>(d_in, d_out);
k<<<1, n>>>();
}
