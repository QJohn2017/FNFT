/*
* This file is part of FNFT.  
*                                                                  
* FNFT is free software; you can redistribute it and/or
* modify it under the terms of the version 2 of the GNU General
* Public License as published by the Free Software Foundation.
*
* FNFT is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*                                                                      
* You should have received a copy of the GNU General Public License
* along with this program. If not, see <http://www.gnu.org/licenses/>.
*
* Contributors:
* Sander Wahls (TU Delft) 2017-2018.
* Peter J Prins (TU Delft) 2017-2018.
*/

#define FNFT_ENABLE_SHORT_NAMES

#include "fnft__errwarn.h"
#include "fnft__poly_fmult.h"
#include "fnft__kdv_fscatter.h"
#include "fnft__kdv_discretization.h"
#include "fnft__misc.h"

/**
 * Returns the length (in number of elements) for "result" in kdv_fscatter
 * or 0 if either the discretization is unknown or D=0.
 */
UINT kdv_fscatter_length(UINT D,
        kdv_discretization_t discretization)
{
    // 2x2 matrix of degree+1 elements for each sample
    return 4*(kdv_discretization_degree(discretization) + 1)*D;
}

/**
 * Calculate M = [cos(eps_t*sqrt(q)),         q * eps * sinc(eps_t*sqrt(q) ;
 *                -eps_t*sinc(eps_t*sqrt(q)), cos(eps_t*sqrt(q))           ];
 */
INT kdv_fscatter_zero_freq_scatter_matrix(COMPLEX *M,
                                                const REAL eps_t, const REAL q)
{
    REAL Delta = eps_t * CSQRT(q);
    M[0] = CCOS(Delta);                // M(1,1) = M(2,2)
    M[2] = -eps_t * misc_CSINC(Delta); // M(2,1)
    M[1] = -q * M[2];                  // M(1,2)
    return 0;
}
/**
 * result needs to be pre-allocated with malloc(kdv_fscatter_length(D,discretization)*sizeof(COMPLEX))
 */
INT kdv_fscatter(const UINT D, COMPLEX const * const q,
                 const REAL eps_t, COMPLEX * const result, UINT * const deg_ptr,
                 kdv_discretization_t discretization)
{
    
    INT i, ret_code, W;
    COMPLEX *p, *p11, *p12, *p21, *p22;
    UINT n, len;
    COMPLEX *e_0_5B, *e_1B, *e_1_5B, *e_2B, *e_3B, *e_4B, *e_5B, *e_6B, *e_8B,
                                        *e_10B, *e_12B, *e_15B, *e_21B, *e_24B,
                                        *e_30B, *e_35B, *e_42B, *e_70B, *e_105B;

    // Check inputs
    if (D == 0)
        return E_INVALID_ARGUMENT(D);
    if (q == NULL)
        return E_INVALID_ARGUMENT(u);
    if (eps_t <= 0.0)
        return E_INVALID_ARGUMENT(eps_t);
    if (result == NULL)
        return E_INVALID_ARGUMENT(result);
    if (deg_ptr == NULL)
        return E_INVALID_ARGUMENT(deg_ptr);

    // Allocate buffers
    len = kdv_fscatter_length(D, discretization);
    if (len == 0) { // size D>0, this means unknown discretization
        return E_INVALID_ARGUMENT(opts->discretization);
    }
    p = malloc(len*sizeof(COMPLEX));
    
    // degree 1 polynomials
    if (p == NULL)
        return E_NOMEM;
    
    // Set the individual scattering matrices up
    *deg_ptr = kdv_discretization_degree(discretization);
    if (*deg_ptr == 0) {
        ret_code = E_INVALID_ARGUMENT(discretization);
        goto release_mem;
    }

    p11 = p;
    p12 = p11 + D*(*deg_ptr+1);
    p21 = p12 + D*(*deg_ptr+1);
    p22 = p21 + D*(*deg_ptr+1);
    
    switch (discretization) {
        case kdv_discretization_2SPLIT1A:
            
            e_1B = malloc(3*sizeof(COMPLEX));
            
            for (i=D-1; i>=0; i--) {
                
                kdv_fscatter_zero_freq_scatter_matrix(e_1B, eps_t/ *deg_ptr, q[i]);
                
                // construct the scattering matrix for the i-th sample
                p11[1] = 0.0;       //coef of z^1
                p11[0] = e_1B[0];   //coef of z^0
                p12[1] = 0.0;
                p12[0] = e_1B[1];
                p21[1] = e_1B[2];
                p21[0] = 0.0;
                p22[1] = e_1B[0];
                p22[0] = 0.0;
                
                p11 += *deg_ptr + 1;
                p21 += *deg_ptr + 1;
                p12 += *deg_ptr + 1;
                p22 += *deg_ptr + 1;
            }
            
            free(e_1B);
            
            break;
            
        case kdv_discretization_2SPLIT1B: //Intentional fallthrough
        case kdv_discretization_2SPLIT2A: //Differs by correction in fnft_kdvv.c
            
            e_1B = malloc(3*sizeof(COMPLEX));
            
            for (i=D-1; i>=0; i--) {
                
                kdv_fscatter_zero_freq_scatter_matrix(e_1B, eps_t/ *deg_ptr, q[i]);
                
                // construct the scattering matrix for the i-th sample
                p11[1] = 0.0;
                p11[0] = e_1B[0];//p22[1];
                p12[1] = e_1B[1];//-q[i]*p21[0];
                p12[0] = 0.0;
                p21[1] = 0.0;                                 //coef of z^1
                p21[0] = e_1B[2];//-eps_t*misc_CSINC(DEL);    //coef of z^0
                p22[1] = e_1B[0];//CCOS(DEL);
                p22[0] = 0.0;
                
                p11 += *deg_ptr + 1;
                p21 += *deg_ptr + 1;
                p12 += *deg_ptr + 1;
                p22 += *deg_ptr + 1;
            }
            
            free(e_1B);
            
            break;
        case kdv_discretization_2SPLIT2B:

            e_0_5B = malloc(3*sizeof(COMPLEX));

            for (i=D-1; i>=0; i--) {

                kdv_fscatter_zero_freq_scatter_matrix(e_0_5B, 0.5*eps_t/ *deg_ptr, q[i]);

                // construct the scattering matrix for the i-th sample
                p11[1] = e_0_5B[1]*e_0_5B[2];
                p11[0] = e_0_5B[0]*e_0_5B[0];
                p12[1] = e_0_5B[0]*e_0_5B[1];
                p12[0] = p12[1];
                p21[1] = e_0_5B[0]*e_0_5B[2];
                p21[0] = p21[1];
                p22[1] = p11[0];
                p22[0] = p11[1];

                p11 += *deg_ptr + 1;
                p21 += *deg_ptr + 1;
                p12 += *deg_ptr + 1;
                p22 += *deg_ptr + 1;
            }

            free(e_0_5B);

            break;

        case kdv_discretization_2SPLIT3A:

            e_1B = malloc(3*sizeof(COMPLEX));
            e_2B = malloc(3*sizeof(COMPLEX));
            e_3B = malloc(3*sizeof(COMPLEX));

            for (i=D-1; i>=0; i--) {

                kdv_fscatter_zero_freq_scatter_matrix(e_1B, eps_t/ *deg_ptr, q[i]);
                kdv_fscatter_zero_freq_scatter_matrix(e_2B, 2*eps_t/ *deg_ptr, q[i]);
                kdv_fscatter_zero_freq_scatter_matrix(e_3B, 3*eps_t/ *deg_ptr, q[i]);

                // construct the scattering matrix for the i-th sample
                p11[3] = 0.0;
                p11[2] = 9*e_1B[2]*e_2B[1]/8;
                p11[1] = 0.0;
                p11[0] = (9*e_1B[0]*e_2B[0] - e_3B[0])/8;
                p12[3] = 0.0;
                p12[2] = 9*e_1B[0]*e_2B[1]/8;
                p12[1] = 0.0;
                p12[0] = (9*e_1B[1]*e_2B[0] - e_3B[1])/8;
                p21[3] = (9*e_1B[2]*e_2B[0] - e_3B[2])/8;
                p21[2] = 0.0;
                p21[1] = 9*e_1B[0]*e_2B[2]/8;
                p21[0] = 0.0;
                p22[3] = p11[0];
                p22[2] = 0.0;
                p22[1] = 9*e_1B[1]*e_2B[2]/8;
                p22[0] = 0.0;

                p11 += *deg_ptr + 1;
                p21 += *deg_ptr + 1;
                p12 += *deg_ptr + 1;
                p22 += *deg_ptr + 1;
            }

            free(e_1B);
            free(e_2B);
            free(e_3B);

            break;
            
        case kdv_discretization_2SPLIT3B:

            e_1B = malloc(3*sizeof(COMPLEX));
            e_2B = malloc(3*sizeof(COMPLEX));
            e_3B = malloc(3*sizeof(COMPLEX));

            for (i=D-1; i>=0; i--) {

                kdv_fscatter_zero_freq_scatter_matrix(e_1B, eps_t/ *deg_ptr, q[i]);
                kdv_fscatter_zero_freq_scatter_matrix(e_2B, 2*eps_t/ *deg_ptr, q[i]);
                kdv_fscatter_zero_freq_scatter_matrix(e_3B, 3*eps_t/ *deg_ptr, q[i]);

                // construct the scattering matrix for the i-th sample
                p11[3] = 0.0;
                p11[2] = 9*e_1B[1]*e_2B[2]/8;
                p11[1] = 0.0;
                p11[0] = (9*e_1B[0]*e_2B[0] - e_3B[0])/8;
                p12[3] = (9*e_1B[1]*e_2B[0] - e_3B[1])/8;
                p12[2] = 0.0;
                p12[1] = 9*e_1B[0]*e_2B[1]/8;
                p12[0] = 0.0;
                p21[3] = 0.0;
                p21[2] = 9*e_1B[0]*e_2B[2]/8;
                p21[1] = 0.0;
                p21[0] = (9*e_1B[2]*e_2B[0] - e_3B[2])/8;
                p22[3] = p11[0];
                p22[2] = 0.0;
                p22[1] = 9*e_1B[2]*e_2B[1]/8;
                p22[0] = 0.0;

                p11 += *deg_ptr + 1;
                p21 += *deg_ptr + 1;
                p12 += *deg_ptr + 1;
                p22 += *deg_ptr + 1;
            }

            free(e_1B);
            free(e_2B);
            free(e_3B);

            break;
        
      case kdv_discretization_2SPLIT4A:

            e_2B = malloc(3*sizeof(COMPLEX));
            e_4B = malloc(3*sizeof(COMPLEX));
            
            for (i=D-1; i>=0; i--) {
                
                kdv_fscatter_zero_freq_scatter_matrix(e_2B, 2*eps_t/ *deg_ptr, q[i]);
                kdv_fscatter_zero_freq_scatter_matrix(e_4B, 4*eps_t/ *deg_ptr, q[i]);
                
                // construct the scattering matrix for the i-th sample
                p11[4] = 0.0;
                p11[3] = 0.0;
                p11[2] = 4*e_2B[1]*e_2B[2]/3;
                p11[1] = 0.0;
                p11[0] = (4*e_2B[0]*e_2B[0] - e_4B[0])/3;
                p12[4] = 0.0;
                p12[3] = 4*e_2B[0]*e_2B[1]/3;
                p12[2] = -e_4B[1]/3;
                p12[1] = p12[3];
                p12[0] = 0.0;
                p21[4] = 0.0;
                p21[3] = 4*e_2B[0]*e_2B[2]/3;
                p21[2] = -e_4B[2]/3;
                p21[1] = p21[3];
                p21[0] = 0.0;
                p22[4] = p11[0];
                p22[3] = 0.0;
                p22[2] = p11[2];
                p22[1] = 0.0;
                p22[0] = 0.0;
                
                p11 += *deg_ptr + 1;
                p21 += *deg_ptr + 1;
                p12 += *deg_ptr + 1;
                p22 += *deg_ptr + 1;
            }
            
            free(e_2B);
            free(e_4B);
            
            break;
        case kdv_discretization_2SPLIT4B:

            e_0_5B = malloc(3*sizeof(COMPLEX));
            e_1B = malloc(3*sizeof(COMPLEX));
            
            for (i=D-1; i>=0; i--) {
                
                kdv_fscatter_zero_freq_scatter_matrix(e_0_5B, 0.5*eps_t/ *deg_ptr, q[i]);
                kdv_fscatter_zero_freq_scatter_matrix(e_1B, eps_t/ *deg_ptr, q[i]);
                
                // construct the scattering matrix for the i-th sample
                p11[2] = (4*e_1B[0]*e_0_5B[1]*e_0_5B[2] - e_1B[1]*e_1B[2])/3;
                p11[1] = 4*(e_1B[1]*e_0_5B[0]*e_0_5B[2] + e_1B[2]*e_0_5B[0]*e_0_5B[1])/3;
                p11[0] = (4*e_1B[0]*e_0_5B[0]*e_0_5B[0] - e_1B[0]*e_1B[0])/3;
                p12[2] = (4*e_1B[0]*e_0_5B[0]*e_0_5B[1] - e_1B[0]*e_1B[1])/3;
                p12[1] = 4*(e_1B[1]*e_0_5B[0]*e_0_5B[0] + e_1B[2]*e_0_5B[1]*e_0_5B[1])/3;
                p12[0] = p12[2];
                p21[2] = (4*e_1B[0]*e_0_5B[0]*e_0_5B[2] - e_1B[0]*e_1B[2])/3;
                p21[1] = 4*(e_1B[2]*e_0_5B[0]*e_0_5B[0] + e_1B[1]*e_0_5B[2]*e_0_5B[2])/3;
                p21[0] = p21[2];
                p22[2] = p11[0];
                p22[1] = p11[1];
                p22[0] = p11[2];
                
                p11 += *deg_ptr + 1;
                p21 += *deg_ptr + 1;
                p12 += *deg_ptr + 1;
                p22 += *deg_ptr + 1;
            }
            
            free(e_0_5B);
            free(e_1B);
            
            break;
        
        case kdv_discretization_2SPLIT5A:
            
            for (n=0; n<len; n++)
                p[n] = 0.0;
            
            e_3B = malloc(3*sizeof(COMPLEX));
            e_5B = malloc(3*sizeof(COMPLEX));
            e_6B = malloc(3*sizeof(COMPLEX));
            e_10B = malloc(3*sizeof(COMPLEX));
            e_15B = malloc(3*sizeof(COMPLEX));
            
            for (i=D-1; i>=0; i--) {
                
                kdv_fscatter_zero_freq_scatter_matrix(e_3B, 3*eps_t/ *deg_ptr, q[i]);
                kdv_fscatter_zero_freq_scatter_matrix(e_5B, 5*eps_t/ *deg_ptr, q[i]);
                kdv_fscatter_zero_freq_scatter_matrix(e_6B, 6*eps_t/ *deg_ptr, q[i]);
                kdv_fscatter_zero_freq_scatter_matrix(e_10B, 10*eps_t/ *deg_ptr, q[i]);
                kdv_fscatter_zero_freq_scatter_matrix(e_15B, 15*eps_t/ *deg_ptr, q[i]);
                
                // construct the scattering matrix for the i-th sample
                
                //p11
                p11[12] = 625*e_3B[2]*e_6B[0]*e_6B[1]/384;
                p11[10] = -81*e_5B[2]*e_10B[1]/128;
                p11[6]  = 625*(e_3B[0]*e_6B[1]*e_6B[2] + e_3B[2]*e_6B[0]*e_6B[1])/384;
                p11[0]  = 625*e_3B[0]*e_6B[0]*e_6B[0]/384 + e_15B[0]/192 - 81*e_5B[0]*e_10B[0]/128;
                
                //p12
                p12[12] = 625*e_3B[0]*e_6B[0]*e_6B[1]/384;
                p12[10] = -81*e_5B[0]*e_10B[1]/128;
                p12[6]  = 625*(e_3B[0]*e_6B[0]*e_6B[1] + e_3B[1]*e_6B[1]*e_6B[2])/384;
                p12[0]  = 625*e_3B[1]*e_6B[0]*e_6B[0]/384 + e_15B[1]/192 - 81*e_5B[1]*e_10B[0]/128;
                
                //p21
                p21[15] = 625*e_3B[2]*e_6B[0]*e_6B[0]/384 + e_15B[2]/192 - 81*e_5B[2]*e_10B[0]/128;
                p21[9]  = 625*(e_3B[0]*e_6B[0]*e_6B[2] + e_3B[2]*e_6B[1]*e_6B[2])/384;
                p21[5]  = -81*e_5B[0]*e_10B[2]/128;
                p21[3]  = 625*e_3B[0]*e_6B[0]*e_6B[2]/384;
                
                //p22
                p22[15] = p11[0];
                p22[9]  = 625*(e_3B[0]*e_6B[1]*e_6B[2] + e_3B[1]*e_6B[0]*e_6B[2])/384;
                p22[5]  = -81*e_5B[1]*e_10B[2]/128;
                p22[3]  = 625*e_3B[1]*e_6B[0]*e_6B[2]/384;
                
                p11 += *deg_ptr + 1;
                p21 += *deg_ptr + 1;
                p12 += *deg_ptr + 1;
                p22 += *deg_ptr + 1;
            }
            
            free(e_3B);
            free(e_5B);
            free(e_6B);
            free(e_10B);
            free(e_15B);
            
            break;
            
        case kdv_discretization_2SPLIT5B:
            
            for (n=0; n<len; n++)
                p[n] = 0.0;
            
            e_3B = malloc(3*sizeof(COMPLEX));
            e_5B = malloc(3*sizeof(COMPLEX));
            e_6B = malloc(3*sizeof(COMPLEX));
            e_10B = malloc(3*sizeof(COMPLEX));
            e_15B = malloc(3*sizeof(COMPLEX));
            
            for (i=D-1; i>=0; i--) {
                
                kdv_fscatter_zero_freq_scatter_matrix(e_3B, 3*eps_t/ *deg_ptr, q[i]);
                kdv_fscatter_zero_freq_scatter_matrix(e_5B, 5*eps_t/ *deg_ptr, q[i]);
                kdv_fscatter_zero_freq_scatter_matrix(e_6B, 6*eps_t/ *deg_ptr, q[i]);
                kdv_fscatter_zero_freq_scatter_matrix(e_10B, 10*eps_t/ *deg_ptr, q[i]);
                kdv_fscatter_zero_freq_scatter_matrix(e_15B, 15*eps_t/ *deg_ptr, q[i]);
                
                // construct the scattering matrix for the i-th sample
                
                //p11
                p11[12] = 625*e_3B[1]*e_6B[0]*e_6B[2]/384;
                p11[10] = -81*e_5B[1]*e_10B[2]/128;
                p11[6]  = 625*(e_3B[0]*e_6B[1]*e_6B[2] + e_3B[1]*e_6B[0]*e_6B[2])/384;
                p11[0]  = 625*e_3B[0]*e_6B[0]*e_6B[0]/384 + e_15B[0]/192 - 81*e_5B[0]*e_10B[0]/128;
                
                //p12
                p12[15] = 625*e_3B[1]*e_6B[0]*e_6B[0]/384 + e_15B[1]/192 - 81*e_5B[1]*e_10B[0]/128;
                p12[9]  = 625*(e_3B[0]*e_6B[0]*e_6B[1] + e_3B[1]*e_6B[1]*e_6B[2])/384;
                p12[5]  = -81*e_5B[0]*e_10B[1]/128;
                p12[3]  = 625*e_3B[0]*e_6B[0]*e_6B[1]/384;
                
                //p21
                p21[12] = 625*e_3B[0]*e_6B[0]*e_6B[2]/384;
                p21[10] = -81*e_5B[0]*e_10B[2]/128;
                p21[6]  = 625*(e_3B[0]*e_6B[0]*e_6B[2] + e_3B[2]*e_6B[1]*e_6B[2])/384;
                p21[0]  = 625*e_3B[2]*e_6B[0]*e_6B[0]/384 + e_15B[2]/192 - 81*e_5B[2]*e_10B[0]/128;
                
                //p22
                p22[15] = p11[0];
                p22[9]  = 625*(e_3B[0]*e_6B[1]*e_6B[2] + e_3B[2]*e_6B[0]*e_6B[1])/384;
                p22[5]  = -81*e_5B[2]*e_10B[1]/128;
                p22[3]  = 625*e_3B[2]*e_6B[0]*e_6B[1]/384;
                
                p11 += *deg_ptr + 1;
                p21 += *deg_ptr + 1;
                p12 += *deg_ptr + 1;
                p22 += *deg_ptr + 1;
            }
            
            free(e_3B);
            free(e_5B);
            free(e_6B);
            free(e_10B);
            free(e_15B);
            
            break;
            
        case kdv_discretization_2SPLIT6A:
            
            for (n=0; n<len; n++)
                p[n] = 0.0;
            
            e_4B = malloc(3*sizeof(COMPLEX));
            e_6B = malloc(3*sizeof(COMPLEX));
            e_12B = malloc(3*sizeof(COMPLEX));

            for (i=D-1; i>=0; i--) {

                kdv_fscatter_zero_freq_scatter_matrix(e_4B, 4*eps_t/ *deg_ptr, q[i]);
                kdv_fscatter_zero_freq_scatter_matrix(e_6B, 6*eps_t/ *deg_ptr, q[i]);
                kdv_fscatter_zero_freq_scatter_matrix(e_12B, 12*eps_t/ *deg_ptr, q[i]);

                // construct the scattering matrix for the i-th sample
                //p11
                p11[8]  = 81*e_4B[0]*e_4B[1]*e_4B[2]/40;
                p11[6]  = -16*e_6B[1]*e_6B[2]/15;
                p11[4]  = 2*p11[8];
                p11[0]  = 81*e_4B[0]*e_4B[0]*e_4B[0]/40 + e_12B[0]/24 - 16*e_6B[0]*e_6B[0]/15;
                
                //p12
                p12[10] = 81*e_4B[0]*e_4B[0]*e_4B[1]/40;
                p12[9]  = -16*e_6B[0]*e_6B[1]/15;
                p12[6]  = p12[10] + 81*e_4B[2]*e_4B[1]*e_4B[1]/40 + e_12B[1]/24;
                p12[3]  = p12[9];
                p12[2]  = p12[10];
                
                //p21
                p21[10] = 81*e_4B[0]*e_4B[0]*e_4B[2]/40;
                p21[9]  = -16*e_6B[0]*e_6B[2]/15;
                p21[6]  = p21[10] + 81*e_4B[1]*e_4B[2]*e_4B[2]/40 + e_12B[2]/24;
                p21[3]  = p21[9];
                p21[2]  = p21[10];
                
                //p22
                p22[12] = p11[0];
                p22[8]  = p11[4];
                p22[6]  = p11[6];
                p22[4]  = p11[8];

                p11 += *deg_ptr + 1;
                p21 += *deg_ptr + 1;
                p12 += *deg_ptr + 1;
                p22 += *deg_ptr + 1;
            }

            free(e_4B);
            free(e_6B);
            free(e_12B);

            break;
        
        case kdv_discretization_2SPLIT6B:

            for (n=0; n<len; n++)
                p[n] = 0.0;
            
            e_1B = malloc(3*sizeof(COMPLEX));
            e_1_5B = malloc(3*sizeof(COMPLEX));
            e_2B = malloc(3*sizeof(COMPLEX));
            e_3B = malloc(3*sizeof(COMPLEX));

            for (i=D-1; i>=0; i--) {

                kdv_fscatter_zero_freq_scatter_matrix(e_1B, eps_t/ *deg_ptr, q[i]);
                kdv_fscatter_zero_freq_scatter_matrix(e_1_5B, 1.5*eps_t/ *deg_ptr, q[i]);
                kdv_fscatter_zero_freq_scatter_matrix(e_2B, 2*eps_t/ *deg_ptr, q[i]);
                kdv_fscatter_zero_freq_scatter_matrix(e_3B, 3*eps_t/ *deg_ptr, q[i]);

                // construct the scattering matrix for the i-th sample

                //p11
                p11[6] = 81*e_1B[1]*e_1B[2]*e_2B[0]*e_2B[0]/40 + e_3B[1]*e_3B[2]/24 - 16*e_3B[0]*e_1_5B[1]*e_1_5B[2]/15;
                p11[4] = 81*(e_1B[0]*e_1B[1]*e_2B[0]*e_2B[2] + e_1B[0]*e_1B[2]*e_2B[0]*e_2B[1] + e_1B[1]*e_1B[2]*e_2B[1]*e_2B[2])/40;
                p11[3] = -16*(e_3B[1]*e_1_5B[0]*e_1_5B[2] + e_3B[2]*e_1_5B[0]*e_1_5B[1])/15;
                p11[2] = 81*(e_1B[0]*e_1B[0]*e_2B[1]*e_2B[2] + e_1B[0]*e_1B[1]*e_2B[0]*e_2B[2] + e_1B[0]*e_1B[2]*e_2B[0]*e_2B[1])/40;
                p11[0] = 81*e_1B[0]*e_1B[0]*e_2B[0]*e_2B[0]/40 + e_3B[0]*e_3B[0]/24 - 16*e_1_5B[0]*e_1_5B[0]*e_3B[0]/15;
                
                //p12
                p12[6] = 81*e_1B[0]*e_1B[1]*e_2B[0]*e_2B[0]/40 + e_3B[0]*e_3B[1]/24 - 16*e_3B[0]*e_1_5B[0]*e_1_5B[1]/15;
                p12[4] = 81*(e_2B[0]*e_2B[1]*e_1B[0]*e_1B[0] + e_2B[1]*e_2B[2]*e_1B[0]*e_1B[1] + e_2B[0]*e_2B[2]*e_1B[1]*e_1B[1])/40;
                p12[3] = -16*(e_3B[1]*e_1_5B[0]*e_1_5B[0] + e_3B[2]*e_1_5B[1]*e_1_5B[1])/15;
                p12[2] = p12[4];
                p12[0] = p12[6];
                
                //p21
                p21[6] = 81*e_1B[0]*e_1B[2]*e_2B[0]*e_2B[0]/40 + e_3B[0]*e_3B[2]/24 - 16*e_3B[0]*e_1_5B[0]*e_1_5B[2]/15;
                p21[4] = 81*(e_2B[0]*e_2B[2]*e_1B[0]*e_1B[0] + e_2B[1]*e_2B[2]*e_1B[0]*e_1B[2] + e_2B[0]*e_2B[1]*e_1B[2]*e_1B[2])/40;
                p21[3] = -16*(e_3B[2]*e_1_5B[0]*e_1_5B[0] + e_3B[1]*e_1_5B[2]*e_1_5B[2])/15;
                p21[2] = p21[4];
                p21[0] = p21[6];
                
                //p22
                p22[6] = p11[0];
                p22[4] = p11[2];
                p22[3] = p11[3];
                p22[2] = p11[4];
                p22[0] = p11[6];
                
                p11 += *deg_ptr + 1;
                p21 += *deg_ptr + 1;
                p12 += *deg_ptr + 1;
                p22 += *deg_ptr + 1;
            }
            
            free(e_1B);
            free(e_1_5B);
            free(e_2B);
            free(e_3B);

            break;
            
        case kdv_discretization_2SPLIT7A:

            for (n=0; n<len; n++)
                p[n] = 0.0;
            
            e_15B = malloc(3*sizeof(COMPLEX));
            e_21B = malloc(3*sizeof(COMPLEX));
            e_30B = malloc(3*sizeof(COMPLEX));
            e_35B = malloc(3*sizeof(COMPLEX));
            e_42B = malloc(3*sizeof(COMPLEX));
            e_70B = malloc(3*sizeof(COMPLEX));
            e_105B = malloc(3*sizeof(COMPLEX));
            
            for (i=D-1; i>=0; i--) {
                
                kdv_fscatter_zero_freq_scatter_matrix(e_15B, 15*eps_t/ *deg_ptr, q[i]);
                kdv_fscatter_zero_freq_scatter_matrix(e_21B, 21*eps_t/ *deg_ptr, q[i]);
                kdv_fscatter_zero_freq_scatter_matrix(e_30B, 30*eps_t/ *deg_ptr, q[i]);
                kdv_fscatter_zero_freq_scatter_matrix(e_35B, 35*eps_t/ *deg_ptr, q[i]);
                kdv_fscatter_zero_freq_scatter_matrix(e_42B, 42*eps_t/ *deg_ptr, q[i]);
                kdv_fscatter_zero_freq_scatter_matrix(e_70B, 70*eps_t/ *deg_ptr, q[i]);
                kdv_fscatter_zero_freq_scatter_matrix(e_105B, 105*eps_t/ *deg_ptr, q[i]);
                
                // construct the scattering matrix for the i-th sample
                
                //p11
                p11[90] = 2874587633249303*e_15B[2]*e_30B[0]*e_30B[0]*e_30B[1]/1125899906842624;
                p11[84] = -15625*e_21B[2]*e_42B[0]*e_42B[1]/9216;
                p11[70] = 729*e_35B[2]*e_70B[1]/5120;
                p11[60] = 2874587633249303*(e_15B[2]*e_30B[0]*e_30B[0]*e_30B[1] + e_15B[0]*e_30B[2]*e_30B[0]*e_30B[1] + e_15B[2]*e_30B[2]*e_30B[1]*e_30B[1])/1125899906842624;
                p11[42] = -15625*(e_21B[0]*e_42B[1]*e_42B[2] + e_21B[2]*e_42B[0]*e_42B[1])/9216;
                p11[30] = 2874587633249303*(e_15B[2]*e_30B[1]*e_30B[0]*e_30B[0] + 2*e_15B[0]*e_30B[1]*e_30B[2]*e_30B[0])/1125899906842624;
                p11[0]  = 2874587633249303*e_15B[0]*e_30B[0]*e_30B[0]*e_30B[0]/1125899906842624 - e_105B[0]/9216 + 729*e_35B[0]*e_70B[0]/5120 - 15625*e_21B[0]*e_42B[0]*e_42B[0]/9216;
                
                //p12
                p12[90] = 2874587633249303*e_15B[0]*e_30B[0]*e_30B[0]*e_30B[1]/1125899906842624;
                p12[84] = -15625*e_21B[0]*e_42B[0]*e_42B[1]/9216;
                p12[70] = 729*e_35B[0]*e_70B[1]/5120;
                p12[60] = 2874587633249303*(e_15B[0]*e_30B[0]*e_30B[0]*e_30B[1] + e_15B[1]*e_30B[2]*e_30B[0]*e_30B[1] + e_15B[0]*e_30B[2]*e_30B[1]*e_30B[1])/1125899906842624;
                p12[42] = -15625*(e_21B[0]*e_42B[0]*e_42B[1] + e_21B[1]*e_42B[1]*e_42B[2])/9216;
                p12[30] = 2874587633249303*(e_15B[0]*e_30B[1]*e_30B[0]*e_30B[0] + 2*e_15B[1]*e_30B[1]*e_30B[2]*e_30B[0])/1125899906842624;
                p12[0]  = 2874587633249303*e_15B[1]*e_30B[0]*e_30B[0]*e_30B[0]/1125899906842624 - e_105B[1]/9216 + 729*e_35B[1]*e_70B[0]/5120 - 15625*e_21B[1]*e_42B[0]*e_42B[0]/9216;
                
                //p21
                p21[105] = 2874587633249303*e_15B[2]*e_30B[0]*e_30B[0]*e_30B[0]/1125899906842624 - e_105B[2]/9216 + 729*e_35B[2]*e_70B[0]/5120 - 15625*e_21B[2]*e_42B[0]*e_42B[0]/9216;
                p21[75]  = 2874587633249303*(e_15B[0]*e_30B[2]*e_30B[0]*e_30B[0] + 2*e_15B[2]*e_30B[1]*e_30B[2]*e_30B[0])/1125899906842624;
                p21[63]  = -15625*(e_21B[0]*e_42B[0]*e_42B[2] + e_21B[2]*e_42B[1]*e_42B[2])/9216;
                p21[45]  = 2874587633249303*(e_15B[0]*e_30B[0]*e_30B[0]*e_30B[2] + e_15B[2]*e_30B[1]*e_30B[0]*e_30B[2] + e_15B[0]*e_30B[1]*e_30B[2]*e_30B[2])/1125899906842624;
                p21[35]  = 729*e_35B[0]*e_70B[2]/5120;
                p21[21]  = -15625*e_21B[0]*e_42B[0]*e_42B[2]/9216;
                p21[15]  = 2874587633249303*e_15B[0]*e_30B[0]*e_30B[0]*e_30B[2]/1125899906842624;
                
                //p22
                p22[105] = p11[0];
                p22[75]  = 2874587633249303*(e_15B[1]*e_30B[2]*e_30B[0]*e_30B[0] + 2*e_15B[0]*e_30B[1]*e_30B[2]*e_30B[0])/1125899906842624;
                p22[63]  = -15625*(e_21B[0]*e_42B[1]*e_42B[2] + e_21B[1]*e_42B[0]*e_42B[2])/9216;
                p22[45]  = 2874587633249303*(e_15B[1]*e_30B[0]*e_30B[0]*e_30B[2] + e_15B[0]*e_30B[1]*e_30B[0]*e_30B[2] + e_15B[1]*e_30B[1]*e_30B[2]*e_30B[2])/1125899906842624;
                p22[35]  = 729*e_35B[1]*e_70B[2]/5120;
                p22[21]  = -15625*e_21B[1]*e_42B[0]*e_42B[2]/9216;
                p22[15]  = 2874587633249303*e_15B[1]*e_30B[0]*e_30B[0]*e_30B[2]/1125899906842624;
                
                p11 += *deg_ptr + 1;
                p21 += *deg_ptr + 1;
                p12 += *deg_ptr + 1;
                p22 += *deg_ptr + 1;
            }
            
            free(e_15B);
            free(e_21B);
            free(e_30B);
            free(e_35B);
            free(e_42B);
            free(e_70B);
            free(e_105B);

            break;
            
        case kdv_discretization_2SPLIT7B:

            for (n=0; n<len; n++)
                p[n] = 0.0;
            
            e_15B = malloc(3*sizeof(COMPLEX));
            e_21B = malloc(3*sizeof(COMPLEX));
            e_30B = malloc(3*sizeof(COMPLEX));
            e_35B = malloc(3*sizeof(COMPLEX));
            e_42B = malloc(3*sizeof(COMPLEX));
            e_70B = malloc(3*sizeof(COMPLEX));
            e_105B = malloc(3*sizeof(COMPLEX));
            
            for (i=D-1; i>=0; i--) {
                
                kdv_fscatter_zero_freq_scatter_matrix(e_15B, 15*eps_t/ *deg_ptr, q[i]);
                kdv_fscatter_zero_freq_scatter_matrix(e_21B, 21*eps_t/ *deg_ptr, q[i]);
                kdv_fscatter_zero_freq_scatter_matrix(e_30B, 30*eps_t/ *deg_ptr, q[i]);
                kdv_fscatter_zero_freq_scatter_matrix(e_35B, 35*eps_t/ *deg_ptr, q[i]);
                kdv_fscatter_zero_freq_scatter_matrix(e_42B, 42*eps_t/ *deg_ptr, q[i]);
                kdv_fscatter_zero_freq_scatter_matrix(e_70B, 70*eps_t/ *deg_ptr, q[i]);
                kdv_fscatter_zero_freq_scatter_matrix(e_105B, 105*eps_t/ *deg_ptr, q[i]);
                
                // construct the scattering matrix for the i-th sample
                
                //p11
                p11[90]  = 2874587633249303*e_15B[1]*e_30B[0]*e_30B[0]*e_30B[2]/1125899906842624;
                p11[84]  = -15625*e_21B[1]*e_42B[0]*e_42B[2]/9216;
                p11[70]  = 729*e_35B[1]*e_70B[2]/5120;
                p11[60]  = 2874587633249303*(e_15B[1]*e_30B[0]*e_30B[0]*e_30B[2] + e_15B[0]*e_30B[1]*e_30B[0]*e_30B[2] + e_15B[1]*e_30B[1]*e_30B[2]*e_30B[2])/1125899906842624;
                p11[42]  = -15625*(e_21B[0]*e_42B[1]*e_42B[2] + e_21B[1]*e_42B[0]*e_42B[2])/9216;
                p11[30]  = 2874587633249303*(e_15B[1]*e_30B[2]*e_30B[0]*e_30B[0] + 2*e_15B[0]*e_30B[1]*e_30B[2]*e_30B[0])/1125899906842624;
                p11[0]   = 2874587633249303*e_15B[0]*e_30B[0]*e_30B[0]*e_30B[0]/1125899906842624 - e_105B[0]/9216 + 729*e_35B[0]*e_70B[0]/5120 - 15625*e_21B[0]*e_42B[0]*e_42B[0]/9216;
                
                //p12
                p12[105] = 2874587633249303*e_15B[1]*e_30B[0]*e_30B[0]*e_30B[0]/1125899906842624 - e_105B[1]/9216 + 729*e_35B[1]*e_70B[0]/5120 - 15625*e_21B[1]*e_42B[0]*e_42B[0]/9216;
                p12[75]  = 2874587633249303*(e_15B[0]*e_30B[1]*e_30B[0]*e_30B[0] + 2*e_15B[1]*e_30B[1]*e_30B[2]*e_30B[0])/1125899906842624;
                p12[63]  = -15625*(e_21B[0]*e_42B[0]*e_42B[1] + e_21B[1]*e_42B[1]*e_42B[2])/9216;
                p12[45]  = 2874587633249303*(e_15B[0]*e_30B[0]*e_30B[0]*e_30B[1] + e_15B[1]*e_30B[2]*e_30B[0]*e_30B[1] + e_15B[0]*e_30B[2]*e_30B[1]*e_30B[1])/1125899906842624;
                p12[35]  = 729*e_35B[0]*e_70B[1]/5120;
                p12[21]  = -15625*e_21B[0]*e_42B[0]*e_42B[1]/9216;
                p12[15]  = 2874587633249303*e_15B[0]*e_30B[0]*e_30B[0]*e_30B[1]/1125899906842624;
                
                //p21
                p21[90]  = 2874587633249303*e_15B[0]*e_30B[0]*e_30B[0]*e_30B[2]/1125899906842624;
                p21[84]  = -15625*e_21B[0]*e_42B[0]*e_42B[2]/9216;
                p21[70]  = 729*e_35B[0]*e_70B[2]/5120;
                p21[60]  = 2874587633249303*(e_15B[0]*e_30B[0]*e_30B[0]*e_30B[2] + e_15B[2]*e_30B[1]*e_30B[0]*e_30B[2] + e_15B[0]*e_30B[1]*e_30B[2]*e_30B[2])/1125899906842624;
                p21[42]  = -15625*(e_21B[0]*e_42B[0]*e_42B[2] + e_21B[2]*e_42B[1]*e_42B[2])/9216;
                p21[30]  = 2874587633249303*(e_15B[0]*e_30B[2]*e_30B[0]*e_30B[0] + 2*e_15B[2]*e_30B[1]*e_30B[2]*e_30B[0])/1125899906842624;
                p21[0]   = 2874587633249303*e_15B[2]*e_30B[0]*e_30B[0]*e_30B[0]/1125899906842624 - e_105B[2]/9216 + 729*e_35B[2]*e_70B[0]/5120 - 15625*e_21B[2]*e_42B[0]*e_42B[0]/9216;
                
                //p22
                p22[105] = 2874587633249303*e_15B[0]*e_30B[0]*e_30B[0]*e_30B[0]/1125899906842624 - e_105B[0]/9216 + 729*e_35B[0]*e_70B[0]/5120 - 15625*e_21B[0]*e_42B[0]*e_42B[0]/9216;
                p22[75]  = 2874587633249303*(e_15B[2]*e_30B[1]*e_30B[0]*e_30B[0] + 2*e_15B[0]*e_30B[1]*e_30B[2]*e_30B[0])/1125899906842624;
                p22[63]  = -15625*(e_21B[0]*e_42B[1]*e_42B[2] + e_21B[2]*e_42B[0]*e_42B[1])/9216;
                p22[45]  = 2874587633249303*(e_15B[2]*e_30B[0]*e_30B[0]*e_30B[1] + e_15B[0]*e_30B[2]*e_30B[0]*e_30B[1] + e_15B[2]*e_30B[2]*e_30B[1]*e_30B[1])/1125899906842624;
                p22[35]  = 729*e_35B[2]*e_70B[1]/5120;
                p22[21]  = -15625*e_21B[2]*e_42B[0]*e_42B[1]/9216;
                p22[15]  = 2874587633249303*e_15B[2]*e_30B[0]*e_30B[0]*e_30B[1]/1125899906842624;
                
                p11 += *deg_ptr + 1;
                p21 += *deg_ptr + 1;
                p12 += *deg_ptr + 1;
                p22 += *deg_ptr + 1;
            }
            
            free(e_15B);
            free(e_21B);
            free(e_30B);
            free(e_35B);
            free(e_42B);
            free(e_70B);
            free(e_105B);
            
            break;

        case kdv_discretization_2SPLIT8A:
            
            for (n=0; n<len; n++)
                p[n] = 0.0;
            
            e_6B = malloc(3*sizeof(COMPLEX));
            e_8B = malloc(3*sizeof(COMPLEX));
            e_12B = malloc(3*sizeof(COMPLEX));
            e_24B = malloc(3*sizeof(COMPLEX));

            for (i=D-1; i>=0; i--) {
                
                kdv_fscatter_zero_freq_scatter_matrix(e_6B, 6*eps_t/ *deg_ptr, q[i]);
                kdv_fscatter_zero_freq_scatter_matrix(e_8B, 8*eps_t/ *deg_ptr, q[i]);
                kdv_fscatter_zero_freq_scatter_matrix(e_12B, 12*eps_t/ *deg_ptr, q[i]);
                kdv_fscatter_zero_freq_scatter_matrix(e_24B, 24*eps_t/ *deg_ptr, q[i]);

                // construct the scattering matrix for the i-th sample
 
                //p11
                p11[18] = 1024*e_6B[0]*e_6B[0]*e_6B[1]*e_6B[2]/315;
                p11[16] = -729*e_8B[0]*e_8B[1]*e_8B[2]/280;
                p11[12] = 2*p11[18] + 1024*e_6B[1]*e_6B[1]*e_6B[2]*e_6B[2]/315 + 16*e_12B[1]*e_12B[2]/45;
                p11[8]  = 2*p11[16];
                p11[6]  = 3*p11[18];
                p11[0]  = 1024*e_6B[0]*e_6B[0]*e_6B[0]*e_6B[0]/315 + 16*e_12B[0]*e_12B[0]/45 - e_24B[0]/360 - 729*e_8B[0]*e_8B[0]*e_8B[0]/280;
                
                //p12
                p12[21] = 1024*e_6B[0]*e_6B[0]*e_6B[0]*e_6B[1]/315;
                p12[20] = -729*e_8B[0]*e_8B[0]*e_8B[1]/280;
                p12[18] = 16*e_12B[0]*e_12B[1]/45;
                p12[15] = 1024*e_6B[0]*e_6B[0]*e_6B[0]*e_6B[1]/315 + 2048*e_6B[2]*e_6B[0]*e_6B[1]*e_6B[1]/315;
                p12[12] = - e_24B[1]/360 + p12[20] - 729*e_8B[1]*e_8B[1]*e_8B[2]/280;
                p12[9]  = p12[15];
                p12[6]  = p12[18];
                p12[4]  = p12[20];
                p12[3]  = p12[21] ;
                
                //p21
                p21[21] = 1024*e_6B[0]*e_6B[0]*e_6B[0]*e_6B[2]/315;
                p21[20] = -729*e_8B[0]*e_8B[0]*e_8B[2]/280;
                p21[18] = 16*e_12B[0]*e_12B[2]/45;
                p21[15] = 1024*e_6B[0]*e_6B[0]*e_6B[0]*e_6B[2]/315 + 2048*e_6B[1]*e_6B[0]*e_6B[2]*e_6B[2]/315;
                p21[12] = - e_24B[2]/360 + p21[20] - 729*e_8B[1]*e_8B[2]*e_8B[2]/280;
                p21[9]  = p21[15];
                p21[6]  = p21[18];
                p21[4]  = p21[20];
                p21[3]  = p21[21];
                
                //p22
                p22[24] = p11[0];
                p22[18] = p11[6];
                p22[16] = p11[8];
                p22[12] = p11[12];
                p22[8]  = p11[16];
                p22[6]  = p11[18];
                
                p11 += *deg_ptr + 1;
                p21 += *deg_ptr + 1;
                p12 += *deg_ptr + 1;
                p22 += *deg_ptr + 1;
            }
            
            free(e_6B);
            free(e_8B);
            free(e_12B);
            free(e_24B);

            break;
            
        case kdv_discretization_2SPLIT8B:
            
            for (n=0; n<len; n++)
                p[n] = 0.0;
            
            e_1_5B = malloc(3*sizeof(COMPLEX));
            e_2B = malloc(3*sizeof(COMPLEX));
            e_3B = malloc(3*sizeof(COMPLEX));
            e_4B = malloc(3*sizeof(COMPLEX));
            e_6B = malloc(3*sizeof(COMPLEX));

            for (i=D-1; i>=0; i--) {

                kdv_fscatter_zero_freq_scatter_matrix(e_1_5B, 1.5*eps_t/ *deg_ptr, q[i]);
                kdv_fscatter_zero_freq_scatter_matrix(e_2B, 2*eps_t/ *deg_ptr, q[i]);
                kdv_fscatter_zero_freq_scatter_matrix(e_3B, 3*eps_t/ *deg_ptr, q[i]);
                kdv_fscatter_zero_freq_scatter_matrix(e_4B, 4*eps_t/ *deg_ptr, q[i]);
                kdv_fscatter_zero_freq_scatter_matrix(e_6B, 6*eps_t/ *deg_ptr, q[i]);

                // construct the scattering matrix for the i-th sample
                
                //p11
                p11[12] = 1024*e_1_5B[1]*e_1_5B[2]*e_3B[0]*e_3B[0]*e_3B[0]/315 - e_6B[1]*e_6B[2]/360 + 16*e_3B[1]*e_3B[2]*e_6B[0]/45 - 729*e_2B[1]*e_2B[2]*e_4B[0]*e_4B[0]/280;
                p11[9]  = 1024*(e_3B[0]*e_3B[0]*e_3B[1]*e_1_5B[0]*e_1_5B[2] + e_3B[0]*e_3B[0]*e_3B[2]*e_1_5B[0]*e_1_5B[1] + 2*e_3B[0]*e_3B[1]*e_3B[2]*e_1_5B[1]*e_1_5B[2])/315;
                p11[8]  = -729*(e_2B[0]*e_2B[1]*e_4B[0]*e_4B[2] + e_2B[0]*e_2B[2]*e_4B[0]*e_4B[1] + e_2B[1]*e_2B[2]*e_4B[1]*e_4B[2])/280;
                p11[6]  = 1024*(e_1_5B[2]*e_3B[0]*e_3B[0]*e_3B[1]*e_1_5B[0] + e_1_5B[1]*e_3B[0]*e_3B[0]*e_3B[2]*e_1_5B[0] + e_3B[0]*e_3B[1]*e_3B[2]*e_1_5B[0]*e_1_5B[0] + e_1_5B[1]*e_1_5B[2]*e_3B[0]*e_3B[1]*e_3B[2] + e_1_5B[2]*e_3B[1]*e_3B[1]*e_3B[2]*e_1_5B[0] + e_1_5B[1]*e_3B[1]*e_3B[2]*e_3B[2]*e_1_5B[0])/315 + 16*(e_6B[2]*e_3B[0]*e_3B[1] + e_6B[1]*e_3B[0]*e_3B[2])/45;
                p11[4]  = -729*(e_2B[0]*e_2B[0]*e_4B[1]*e_4B[2] + e_2B[0]*e_2B[1]*e_4B[0]*e_4B[2] + e_2B[0]*e_2B[2]*e_4B[0]*e_4B[1])/280;
                p11[3]  = 1024*(2*e_3B[0]*e_3B[1]*e_3B[2]*e_1_5B[0]*e_1_5B[0] + e_3B[0]*e_3B[0]*e_3B[1]*e_1_5B[0]*e_1_5B[2] + e_3B[0]*e_3B[0]*e_3B[2]*e_1_5B[0]*e_1_5B[1])/315;
                p11[0]  = 1024*e_3B[0]*e_3B[0]*e_3B[0]*e_1_5B[0]*e_1_5B[0]/315 + 16*e_3B[0]*e_3B[0]*e_6B[0]/45 - e_6B[0]*e_6B[0]/360 - 729*e_2B[0]*e_2B[0]*e_4B[0]*e_4B[0]/280;
                
                //p12
                p12[12] = 1024*e_1_5B[0]*e_1_5B[1]*e_3B[0]*e_3B[0]*e_3B[0]/315 + 16*e_3B[1]*e_6B[0]*e_3B[0]/45 - e_6B[0]*e_6B[1]/360 - 729*e_2B[0]*e_2B[1]*e_4B[0]*e_4B[0]/280;
                p12[9]  = 1024*(e_3B[1]*e_3B[0]*e_3B[0]*e_1_5B[0]*e_1_5B[0] + e_3B[2]*e_3B[0]*e_3B[0]*e_1_5B[1]*e_1_5B[1] + 2*e_3B[1]*e_3B[2]*e_3B[0]*e_1_5B[0]*e_1_5B[1])/315;
                p12[8]  = -729*(e_2B[0]*e_2B[0]*e_4B[0]*e_4B[1] + e_2B[1]*e_2B[1]*e_4B[0]*e_4B[2] + e_2B[0]*e_2B[1]*e_4B[1]*e_4B[2])/280;
                p12[6]  = 1024*(e_3B[0]*e_3B[0]*e_3B[1]*e_1_5B[0]*e_1_5B[0] + e_3B[0]*e_3B[0]*e_3B[2]*e_1_5B[1]*e_1_5B[1] + 2*e_3B[0]*e_3B[1]*e_3B[2]*e_1_5B[0]*e_1_5B[1] + e_3B[1]*e_3B[1]*e_3B[2]*e_1_5B[0]*e_1_5B[0] + e_3B[1]*e_3B[2]*e_3B[2]*e_1_5B[1]*e_1_5B[1])/315 + 16*(e_6B[1]*e_3B[0]*e_3B[0] + e_6B[2]*e_3B[1]*e_3B[1])/45;
                p12[4]  = p12[8];
                p12[3]  = p12[9];
                p12[0]  = p12[12];
                
                //p21
                p21[12] = 1024*e_1_5B[0]*e_1_5B[2]*e_3B[0]*e_3B[0]*e_3B[0]/315 + 16*e_3B[2]*e_6B[0]*e_3B[0]/45 - e_6B[0]*e_6B[2]/360 - 729*e_2B[0]*e_2B[2]*e_4B[0]*e_4B[0]/280;
                p21[9]  = 1024*(e_3B[2]*e_3B[0]*e_3B[0]*e_1_5B[0]*e_1_5B[0] + e_3B[1]*e_3B[0]*e_3B[0]*e_1_5B[2]*e_1_5B[2] + 2*e_3B[1]*e_3B[2]*e_3B[0]*e_1_5B[0]*e_1_5B[2])/315;
                p21[8]  = -729*(e_2B[0]*e_2B[0]*e_4B[0]*e_4B[2] + e_2B[2]*e_2B[2]*e_4B[0]*e_4B[1] + e_2B[0]*e_2B[2]*e_4B[1]*e_4B[2])/280;
                p21[6]  = 1024*(e_3B[0]*e_3B[0]*e_3B[1]*e_1_5B[2]*e_1_5B[2] + e_3B[0]*e_3B[0]*e_3B[2]*e_1_5B[0]*e_1_5B[0] + 2*e_3B[0]*e_3B[1]*e_3B[2]*e_1_5B[0]*e_1_5B[2] + e_3B[1]*e_3B[1]*e_3B[2]*e_1_5B[2]*e_1_5B[2] + e_3B[1]*e_3B[2]*e_3B[2]*e_1_5B[0]*e_1_5B[0])/315 + 16*(e_6B[2]*e_3B[0]*e_3B[0] + e_6B[1]*e_3B[2]*e_3B[2])/45;
                p21[4]  = p21[8];
                p21[3]  = p21[9];
                p21[0]  = p21[12];
                
                //p22
                p22[12] = p11[0];
                p22[9]  = p11[3];
                p22[8]  = p11[4];
                p22[6]  = p11[6];
                p22[4]  = p11[8];
                p22[3]  = p11[9];
                p22[0]  = p11[12];
                
                p11 += *deg_ptr + 1;
                p21 += *deg_ptr + 1;
                p12 += *deg_ptr + 1;
                p22 += *deg_ptr + 1;
            }

            free(e_1_5B);
            free(e_2B);
            free(e_3B);
            free(e_4B);
            free(e_6B);
            
            break;
            
        default: // Unknown discretization
            ret_code = E_INVALID_ARGUMENT(discretization);
            goto release_mem;
    }
    // Multiply the individual scattering matrices
    ret_code = poly_fmult2x2(deg_ptr, D, p, result, &W);
    CHECK_RETCODE(ret_code, release_mem);
    
release_mem:
    free(p);
    return ret_code;
}



//                p11[12] = ;
//                p11[11] = ;
//                p11[10] = ;
//                p11[9] = ;
//                p11[8] = ;
//                p11[7] = ;
//                p11[6] = ;
//                p11[5] = ;
//                p11[4] = ;
//                p11[3] = ;
//                p11[2] = ;
//                p11[1] = ;
//                p11[0] = ;
//                p12[12] = ;
//                p12[11] = ;
//                p12[10] = ;
//                p12[9] = ;
//                p12[8] = ;
//                p12[7] = ;
//                p12[6] = ;
//                p12[5] = ;
//                p12[4] = ;
//                p12[3] = ;
//                p12[2] = ;
//                p12[1] = ;
//                p12[0] = ;
//                p21[12] = ;
//                p21[11] = ;
//                p21[10] = ;
//                p21[9] = ;
//                p21[8] = ;
//                p21[7] = ;
//                p21[6] = ;
//                p21[5] = ;
//                p21[4] = ;
//                p21[3] = ;
//                p21[2] = ;
//                p21[1] = ;
//                p21[0] = ;
//                p22[12] = ;
//                p22[11] = ;
//                p22[10] = ;
//                p22[9] = ;
//                p22[8] = ;
//                p22[7] = ;
//                p22[6] = ;
//                p22[5] = ;
//                p22[4] = ;
//                p22[3] = ;
//                p22[2] = ;
//                p22[1] = ;
//                p22[0] = ;
