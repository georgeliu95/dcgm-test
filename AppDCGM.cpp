/* Includes, system */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>

/* Includes, cuda */
#include <cublas_v2.h>
#include <cuda_runtime.h>
#include "helper_cuda.h"

#include "dcgm_structs.h"
#include "dcgm_agent.h"

/* Matrix size */
#define N (275)

inline bool check(dcgmReturn_t res, int iLine, const char *szFile) {
    if (res != DCGM_ST_OK) {
        std::cout << "DCGM runtime API error " << errorString(res) << " at line " << iLine << " in file " << szFile << std::endl;
        return false;
    }
    return true;
}

#define ck(call) check(call, __LINE__, __FILE__)

/* Host implementation of a simple version of sgemm */
static void simple_sgemm(int n, float alpha, const float *A, const float *B,
                         float beta, float *C)
{
    int i;
    int j;
    int k;

    for (i = 0; i < n; ++i)
    {
        for (j = 0; j < n; ++j)
        {
            float prod = 0;

            for (k = 0; k < n; ++k)
            {
                prod += A[k * n + i] * B[j * n + k];
            }

            C[j * n + i] = alpha * prod + beta * C[j * n + i];
        }
    }
}

/* Main */
int main(int argc, char **argv)
{

    dcgmReturn_t result;
    dcgmHandle_t dcgmHandle = reinterpret_cast<dcgmHandle_t>(nullptr);
    dcgmProfGetMetricGroups_t metricGroups;
    std::memset(&metricGroups, 0, sizeof(metricGroups));
    dcgmGpuGrp_t groupId;

    ck(dcgmInit());
    ck(dcgmStartEmbedded(DCGM_OPERATION_MODE_AUTO, &dcgmHandle));
    ck(dcgmGroupCreate(dcgmHandle, DCGM_GROUP_DEFAULT, const_cast<char*>("test"), &groupId));
    
    metricGroups.version = dcgmProfGetMetricGroups_version;
    metricGroups.groupId = groupId;

    ck(dcgmProfGetSupportedMetricGroups(dcgmHandle, &metricGroups));
    printf("Num of metricGroups is %u\n", metricGroups.numMetricGroups);

    dcgmProfWatchFields_t watchFields;
    std::memset(&watchFields, 0, sizeof(watchFields));
    watchFields.version = dcgmProfWatchFields_version;
    watchFields.groupId = groupId;
    watchFields.maxKeepAge = 15;
    watchFields.fieldIds[0] = DCGM_FI_PROF_SM_ACTIVE;
    watchFields.numFieldIds = 1;
    watchFields.updateFreq = 1000;
    ck(dcgmProfWatchFields(dcgmHandle, &watchFields));

    ck(dcgmUpdateAllFields(dcgmHandle, 1));
    
    dcgmFieldValue_v1 values[DCGM_FI_MAX_FIELDS];
    std::memset(&values[0], 0, sizeof(values));
    ck(dcgmGetLatestValuesForFields(dcgmHandle, 0, watchFields.fieldIds, watchFields.numFieldIds, values));
    std::cout << values[DCGM_FI_PROF_SM_ACTIVE].value.dbl << std::endl;

    cublasStatus_t status;
    float *h_A;
    float *h_B;
    float *h_C;
    float *h_C_ref;
    float *d_A = 0;
    float *d_B = 0;
    float *d_C = 0;
    float alpha = 1.0f;
    float beta = 0.0f;
    int n2 = N * N;
    int i;
    float error_norm;
    float ref_norm;
    float diff;
    cublasHandle_t handle;

    int dev = findCudaDevice(argc, (const char **)argv);

    if (dev == -1)
    {
        return EXIT_FAILURE;
    }

    /* Initialize CUBLAS */
    printf("simpleCUBLAS test running..\n");

    status = cublasCreate(&handle);

    if (status != CUBLAS_STATUS_SUCCESS)
    {
        fprintf(stderr, "!!!! CUBLAS initialization error\n");
        return EXIT_FAILURE;
    }

    /* Allocate host memory for the matrices */
    h_A = reinterpret_cast<float *>(malloc(n2 * sizeof(h_A[0])));

    if (h_A == 0)
    {
        fprintf(stderr, "!!!! host memory allocation error (A)\n");
        return EXIT_FAILURE;
    }

    h_B = reinterpret_cast<float *>(malloc(n2 * sizeof(h_B[0])));

    if (h_B == 0)
    {
        fprintf(stderr, "!!!! host memory allocation error (B)\n");
        return EXIT_FAILURE;
    }

    h_C = reinterpret_cast<float *>(malloc(n2 * sizeof(h_C[0])));

    if (h_C == 0)
    {
        fprintf(stderr, "!!!! host memory allocation error (C)\n");
        return EXIT_FAILURE;
    }

    /* Fill the matrices with test data */
    for (i = 0; i < n2; i++)
    {
        h_A[i] = rand() / static_cast<float>(RAND_MAX);
        h_B[i] = rand() / static_cast<float>(RAND_MAX);
        h_C[i] = rand() / static_cast<float>(RAND_MAX);
    }

    /* Allocate device memory for the matrices */
    if (cudaMalloc(reinterpret_cast<void **>(&d_A), n2 * sizeof(d_A[0])) !=
        cudaSuccess)
    {
        fprintf(stderr, "!!!! device memory allocation error (allocate A)\n");
        return EXIT_FAILURE;
    }

    if (cudaMalloc(reinterpret_cast<void **>(&d_B), n2 * sizeof(d_B[0])) !=
        cudaSuccess)
    {
        fprintf(stderr, "!!!! device memory allocation error (allocate B)\n");
        return EXIT_FAILURE;
    }

    if (cudaMalloc(reinterpret_cast<void **>(&d_C), n2 * sizeof(d_C[0])) !=
        cudaSuccess)
    {
        fprintf(stderr, "!!!! device memory allocation error (allocate C)\n");
        return EXIT_FAILURE;
    }

    /* Initialize the device matrices with the host matrices */
    status = cublasSetVector(n2, sizeof(h_A[0]), h_A, 1, d_A, 1);

    if (status != CUBLAS_STATUS_SUCCESS)
    {
        fprintf(stderr, "!!!! device access error (write A)\n");
        return EXIT_FAILURE;
    }

    status = cublasSetVector(n2, sizeof(h_B[0]), h_B, 1, d_B, 1);

    if (status != CUBLAS_STATUS_SUCCESS)
    {
        fprintf(stderr, "!!!! device access error (write B)\n");
        return EXIT_FAILURE;
    }

    status = cublasSetVector(n2, sizeof(h_C[0]), h_C, 1, d_C, 1);

    if (status != CUBLAS_STATUS_SUCCESS)
    {
        fprintf(stderr, "!!!! device access error (write C)\n");
        return EXIT_FAILURE;
    }

    /* Performs operation using plain C code */
    simple_sgemm(N, alpha, h_A, h_B, beta, h_C);
    h_C_ref = h_C;

    /* Performs operation using cublas */
    status = cublasSgemm(handle, CUBLAS_OP_N, CUBLAS_OP_N, N, N, N, &alpha, d_A,
                         N, d_B, N, &beta, d_C, N);
    ck(dcgmGetLatestValuesForFields(dcgmHandle, 0, watchFields.fieldIds, watchFields.numFieldIds, values));
    std::cout << values[DCGM_FI_PROF_SM_ACTIVE].status << values[DCGM_FI_PROF_SM_ACTIVE].value.dbl << std::endl;

    if (status != CUBLAS_STATUS_SUCCESS)
    {
        fprintf(stderr, "!!!! kernel execution error.\n");
        return EXIT_FAILURE;
    }

    /* Allocate host memory for reading back the result from device memory */
    h_C = reinterpret_cast<float *>(malloc(n2 * sizeof(h_C[0])));

    if (h_C == 0)
    {
        fprintf(stderr, "!!!! host memory allocation error (C)\n");
        return EXIT_FAILURE;
    }

    /* Read the result back */
    status = cublasGetVector(n2, sizeof(h_C[0]), d_C, 1, h_C, 1);

    if (status != CUBLAS_STATUS_SUCCESS)
    {
        fprintf(stderr, "!!!! device access error (read C)\n");
        return EXIT_FAILURE;
    }

    /* Check result against reference */
    error_norm = 0;
    ref_norm = 0;

    for (i = 0; i < n2; ++i)
    {
        diff = h_C_ref[i] - h_C[i];
        error_norm += diff * diff;
        ref_norm += h_C_ref[i] * h_C_ref[i];
    }

    error_norm = static_cast<float>(sqrt(static_cast<double>(error_norm)));
    ref_norm = static_cast<float>(sqrt(static_cast<double>(ref_norm)));

    if (fabs(ref_norm) < 1e-7)
    {
        fprintf(stderr, "!!!! reference norm is 0\n");
        return EXIT_FAILURE;
    }

    /* Memory clean up */
    free(h_A);
    free(h_B);
    free(h_C);
    free(h_C_ref);

    if (cudaFree(d_A) != cudaSuccess)
    {
        fprintf(stderr, "!!!! memory free error (A)\n");
        return EXIT_FAILURE;
    }

    if (cudaFree(d_B) != cudaSuccess)
    {
        fprintf(stderr, "!!!! memory free error (B)\n");
        return EXIT_FAILURE;
    }

    if (cudaFree(d_C) != cudaSuccess)
    {
        fprintf(stderr, "!!!! memory free error (C)\n");
        return EXIT_FAILURE;
    }

    /* Shutdown */
    status = cublasDestroy(handle);

    if (status != CUBLAS_STATUS_SUCCESS)
    {
        fprintf(stderr, "!!!! shutdown error (A)\n");
        return EXIT_FAILURE;
    }

    if (error_norm / ref_norm < 1e-6f)
    {
        printf("simpleCUBLAS test passed.\n");
        // exit(EXIT_SUCCESS);
    }
    else
    {
        printf("simpleCUBLAS test failed.\n");
        exit(EXIT_FAILURE);
    }

    ck(dcgmGetLatestValuesForFields(dcgmHandle, 0, watchFields.fieldIds, watchFields.numFieldIds, values));
    std::cout << values[DCGM_FI_PROF_SM_ACTIVE].status << values[DCGM_FI_PROF_SM_ACTIVE].value.dbl << std::endl;
cleanup:
    std::cout << "Cleaning up. \n";
    // if (deviceConfigList)
    //     delete[] deviceConfigList;
    // dcgmGroupDestroy(dcgmHandle, myGroupId);
    // dcgmStatusDestroy(statusHandle);
    // if (standalone)
    //     dcgmDisconnect(dcgmHandle);
    // else
    //     dcgmStopEmbedded(dcgmHandle);
    ck(dcgmGroupDestroy(dcgmHandle, groupId));
    ck(dcgmStopEmbedded(dcgmHandle));
    ck(dcgmShutdown());
    return EXIT_SUCCESS;
}