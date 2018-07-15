#ifndef SRC_LS3DMULTICUDA_CCUDAAXBSOLVERWRAPPER_H_
#define SRC_LS3DMULTICUDA_CCUDAAXBSOLVERWRAPPER_H_

#include "DataBuffer.cuh"

#include <time.h>

#include <cuda_runtime.h>
#include <cublas_v2.h>
#include <cusolverDn.h>
#include <string>

using namespace std;

class CCUDA_AX_B_SolverWrapper {
private:
	cusolverDnHandle_t handle;
	cublasHandle_t cublasHandle;
	cudaStream_t stream;

public:
	bool CCUDA_AX_B_SolverWrapperDEBUG;

	template <typename T_ELEM>
	int loadMMSparseMatrix(
	    char *filename,
	    char elem_type,
	    bool csrFormat,
	    int *m,
	    int *n,
	    int *nnz,
	    T_ELEM **aVal,
	    int **aRowInd,
	    int **aColInd,
	    int extendSymMatrix);

	enum Solver_Method
	{
	    chol = 0,
	    lu = 1,
	    qr = 2
	};

	enum CCUDA_AX_B_SolverWrapper_error
	{
		success,
		fail_problem_with_CUDA_AX_B_Solver
	};

	CCUDA_AX_B_SolverWrapper(bool _CCUDA_AX_B_SolverWrapperDEBUG, int cuda_device);
	virtual ~CCUDA_AX_B_SolverWrapper();

	double Solve(double *a,double *b,double *x, int a_rows, int a_cols, int b_cols, char method);
	double Compute_AtP(int threads, double *A, double *P, double *AtP, int rows, int columns);
	double Multiply(double *a, double *b,double *c, int a_rows, int a_cols, int b_cols);

	CCUDA_AX_B_SolverWrapper_error Solve_ATPA_ATPl_x(int threads, double *A, double *P, double *l,
			double *x, int rows, int columns, Solver_Method solver_method);

	CCUDA_AX_B_SolverWrapper_error Solve_ATPA_ATPl_x_data_on_GPU(int threads, double *d_A, double *d_P, double *d_l,
			double *x, int rows, int columns, Solver_Method solver_method);
	////////////solvers
	/*
	 *  solve A*x = b by Cholesky factorization
	 *
	 */
	int linearSolverCHOL(
	    cusolverDnHandle_t handle,
	    int n,
	    const double *Acopy,
	    int lda,
	    const double *b,
	    double *x);
	/*
	 *  solve A*x = b by LU with partial pivoting
	 *
	 */
	int linearSolverLU(
	    cusolverDnHandle_t handle,
	    int n,
	    const double *Acopy,
	    int lda,
	    const double *b,
	    double *x);

	/*
	 *  solve A*x = b by QR
	 *
	 */
	int linearSolverQR(
	    cusolverDnHandle_t handle,
	    int n,
	    const double *Acopy,
	    int lda,
	    const double *b,
	    double *x);

	cublasStatus_t multiplyCUBLAS( cublasHandle_t handle, const double *d_a, const double *d_b, double *d_c, int a_rows, int a_cols, int b_cols);

	void cout_cudaError_t(cudaError_t err, string message);
	void cout_cusolverStatus_t(cusolverStatus_t err, string message);
	void cout_cublasStatus_t(cublasStatus_t err, string message);
	void setCCUDA_AX_B_SolverWrapperDEBUG(bool value){this->CCUDA_AX_B_SolverWrapperDEBUG = value;};

	void throw_on_cuda_error(cudaError_t code, const char *file, int line);


	//double *d_A ;
	DataBuffer<double, fallback_allocator> d_A;

	//double *d_x ;
	DataBuffer<double, fallback_allocator> d_x;

	//double *d_b ;
	DataBuffer<double, fallback_allocator> d_b;

	//double *d_P;
	DataBuffer<double, fallback_allocator> d_P;

	//double *d_AtP;
	DataBuffer<double, fallback_allocator> d_AtP;

	//double *d_AtPA;
	DataBuffer<double, fallback_allocator> d_AtPA;

	//double *d_l;
	DataBuffer<double, fallback_allocator> d_l;

	//double *d_AtPl;
	DataBuffer<double, fallback_allocator> d_AtPl;

	//double *d_a;
	DataBuffer<double, fallback_allocator> d_a;

	//double *d_c;
	DataBuffer<double, fallback_allocator> d_c;

	//int *info;
	DataBuffer<int, fallback_allocator> info;

	//double *buffer;
	DataBuffer<double, fallback_allocator> buffer;

	//int *ipiv;
	DataBuffer<int, fallback_allocator> ipiv;

	//double *tau;
	DataBuffer<double, fallback_allocator> tau;

};

#endif /* SRC_LS3DMULTICUDA_CCUDAAXBSOLVERWRAPPER_H_ */
