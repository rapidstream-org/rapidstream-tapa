#include <assert.h>
#include <stdio.h>
#include "kernel_kernel.h"

#include "kernel.h"

void init_array(data_t A[N][N])
{
  int i, j;

  for (i = 0; i < N; i++)
  {
    for (j = 0; j <= i; j++)
      A[i][j] = (data_t)(-j % N) / N + 1;
    for (j = i + 1; j < N; j++) {
      A[i][j] = 0;
    }
    A[i][i] = 1;
  }

  /* Make the matrix positive semi-definite. */
  /* not necessary for LU, but using same code as cholesky */
  int r, s, t;
  data_t B[N][N];
  for (r = 0; r < N; r++)
    for (s = 0; s < N; s++) 
      B[r][s] = 0;
  for (t = 0; t < N; t++)
    for (r = 0; r < N; r++)
      for (s = 0; s < N; s++)
        B[r][s] += A[r][t] * A[s][t];
  for (r = 0; r < N; r++)        
    for (s = 0; s < N; s++)
      A[r][s] = B[r][s];
}

void lu_cpu(data_t A[N][N], data_t L[N][N], data_t U[N][N]) {
  data_t prev_V[N][N][N];
  data_t V_tmp[N][N][N];
  data_t U_tmp[N][N][N];
  data_t L_tmp[N][N][N];

  for (int k = 0; k < N; k++)
    for (int j = k; j < N; j++)
      for (int i = k; i < N; i++) {
        if (k == 0)
          prev_V[i][j][k] = A[i][j];
        else
          prev_V[i][j][k] = V_tmp[i][j][k - 1];
        
        if (j == k) {
          U_tmp[i][j][k] = prev_V[i][j][k];
          U[j][i] = U_tmp[i][j][k];
        } else {
          U_tmp[i][j][k] = U_tmp[i][j - 1][k];

          if (i == k) {            
            L_tmp[i][j][k] = prev_V[i][j][k] / U_tmp[i][j - 1][k]; // final
            L[i][j] = L_tmp[i][j][k];
          } else {
            L_tmp[i][j][k] = L_tmp[i - 1][j][k];
          }
          V_tmp[i][j][k] = prev_V[i][j][k] - L_tmp[i][j][k] * U_tmp[i][j - 1][k];
        }
      }  
}

void lu_device(data_t A[N][N], data_t L[N][N], data_t U[N][N])
{
  {
    // Allocate memory in host memory
    float *dev_A = (float *)malloc((32) * (32) * sizeof(float));
    float *dev_L = (float *)malloc((31) * (32) * sizeof(float));
    float *dev_U = (float *)malloc((32) * (32) * sizeof(float));

    // Initialize host buffers
    memcpy(dev_A, A, (32) * (32) * sizeof(float));
    memcpy(dev_L, L, (31) * (32) * sizeof(float));
    memcpy(dev_U, U, (32) * (32) * sizeof(float));

    // Allocate buffers in device memory
    std::vector<float *> buffer_A;
    std::vector<float *> buffer_L;
    std::vector<U_t16 *> buffer_U;
    for (int i = 0; i < 1; i++) {
      float *buffer_A_tmp = (float *)malloc((32) * (32) * sizeof(float));
      buffer_A.push_back(buffer_A_tmp);
    }
    for (int i = 0; i < 1; i++) {
      float *buffer_L_tmp = (float *)malloc((31) * (32) * sizeof(float));
      buffer_L.push_back(buffer_L_tmp);
    }
    for (int i = 0; i < 1; i++) {
      U_t16 *buffer_U_tmp = (U_t16 *)malloc((32) * (32) * sizeof(float));
      buffer_U.push_back(buffer_U_tmp);
    }

    for (int i = 0; i < 1; i++) {
      memcpy(buffer_A[i], dev_A, (32) * (32) * sizeof(float));
    }

    for (int i = 0; i < 1; i++) {
      memcpy(buffer_L[i], dev_L, (31) * (32) * sizeof(float));
    }

    for (int i = 0; i < 1; i++) {
      memcpy(buffer_U[i], dev_U, (32) * (32) * sizeof(float));
    }

    {
      // Launch the kernel
      kernel0(buffer_A[0], buffer_L[0], buffer_U[0]);
    }
    for (int i = 0; i < 1; i++) {
      memcpy(dev_L, buffer_L[i], (31) * (32) * sizeof(float));
    }

    for (int i = 0; i < 1; i++) {
      memcpy(dev_U, buffer_U[i], (32) * (32) * sizeof(float));
    }

    // Restore data from host buffers
    memcpy(L, dev_L, (31) * (32) * sizeof(float));
    memcpy(U, dev_U, (32) * (32) * sizeof(float));

    // Clean up resources
    for (int i = 0; i < 1; i++) {
      free(buffer_A[i]);
    }
    for (int i = 0; i < 1; i++) {
      free(buffer_L[i]);
    }
    for (int i = 0; i < 1; i++) {
      free(buffer_U[i]);
    }
    free(dev_A);
    free(dev_L);
    free(dev_U);
  }
}

int main(int argc, char **argv) {
  data_t A[N][N], L[N][N], U[N][N], L_golden[N][N], U_golden[N][N];

  init_array(A);
  for (int i = 0; i < N; i++)
    for (int j = 0; j < N; j++) {
      L[i][j] = 0;
      U[i][j] = 0;
      L_golden[i][j] = 0;
      U_golden[i][j] = 0;
    }
    
  lu_device(A, L, U);
  lu_cpu(A, L_golden, U_golden);

  int err = 0;
  for (int i = 0; i < N; i++)
    for (int j = 0; j <= i; j++) {
      if (fabs((float)L_golden[i][j] - (float)L[i][j]) > 0.001)
        err++;
    }
  for (int i = 0; i < N; i++)
    for (int j = i; j < N; j++) {
      if (fabs((float)U_golden[i][j] - (float)U[i][j]) > 0.001)
        err++;
    }

  if (err)
    printf("Failed with %d errors!\n", err);
  else
    printf("Passed!\n");

  printf("A:\n");
  for (int i = 0; i < N; i++) {
    for (int j = 0; j < N; j++) 
      printf("%f ", A[i][j]);
    printf("\n");
  }

  printf("L_golden:\n");
  for (int i = 0; i < N; i++) {
    for (int j = 0; j < N; j++) {      
      printf("%f ", (i == j)? 1.0 : L_golden[j][i]);      
    }
    printf("\n");
  }

  printf("U_golden:\n");
  for (int i = 0; i < N; i++) {
    for (int j = 0; j < N; j++) {
      printf("%f ", U_golden[i][j]);
    }
    printf("\n");
  }

  printf("L:\n");
  for (int i = 0; i < N; i++) {
    for (int j = 0; j < N; j++) {      
      printf("%f ", (i == j)? 1.0 : (j < i)? L[j][i] : 0.0);      
    }
    printf("\n");
  }

  printf("U:\n");
  for (int i = 0; i < N; i++) {
    for (int j = 0; j < N; j++) {
      printf("%f ", (j < i)? 0.0 : U[i][j]);
    }
    printf("\n");
  }

  return 0;    
}
