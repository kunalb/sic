__global__ void add(float* out, float* a, float* b) {
int i = ((blockIdx.x * blockDim.x) + threadIdx.x);
(out[i] = (a[i] + b[i]));
}
__device__ float smooth(float* v) {
__shared__ float tile[256];
(tile[threadIdx.x] = v[threadIdx.x]);
__syncthreads();
return tile[threadIdx.x];
}
