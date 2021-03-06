#include <assert.h>
#include <petsc.h>

#include "matrix.h"
#include "mesh.h"

#define F_x 1.0
#define F_y 1.0

/**
 * Compute stiffness matrix for first order Nedelec element in 2D
 *
 * 1/|det Bk| \int f(x,y) * sign_k * curl_ned_k * sign_l * curl_sign_l dx
 * 1/|det Bk| * sign_k * sign_l \int f(x, y) * curl_ned_k * curl_sign_l dx
 */
static inline PetscReal stiffness_matrix_2D(struct function_space fs,
                                            PetscReal detJ, PetscInt sign_k,
                                            PetscInt sign_l, PetscInt k,
                                            PetscInt l)
{
    PetscReal local = 1 / PetscAbsReal(detJ) * sign_l * sign_k;

    /** This is used only if f(x, y) is non constant
    for (PetscInt i = 0; i < fs->q.size; i++) {
        int _3i = 3 * i;
        sum += q.pw[_3i + 2] * fs.cval[_3i + k] * fs.cval[_3i + l]
             * sctx->stiffness_function_2D(q.pw[_3i + 0], q.pw[_3i + 1]);
    }
    */
    // TODO: handle stiffness_const()
    return local * 0.5 * fs.cval[k] * fs.cval[l];
}

/**
 * Mass matrix computation
 * |det Bk| \int sign_k * Bk^-T * ned_k * sign_l * Bk^-T * ned_l dx
 * |det Bk| * sign_k * sign_l \int Bk * Bk^-T * ned_k * ned_l dx
 */
static inline PetscReal mass_matrix_2D(PetscReal *C, struct function_space fs,
                                       PetscReal detJ, PetscInt sign_k,
                                       PetscInt sign_l, PetscInt k_ned,
                                       PetscInt l_ned)
{
    PetscReal local = PetscAbsReal(detJ) * sign_k * sign_l;
    PetscReal sum = 0;

    for (PetscInt i = 0; i < fs.q.size; i++) {
        int k_off = k_ned * (fs.q.size * 2) + i * 2;
        int l_off = l_ned * (fs.q.size * 2) + i * 2;

        PetscReal __x = C[0] * fs.val[k_off + 0] + C[1] * fs.val[k_off + 1];
        PetscReal __y = C[2] * fs.val[k_off + 0] + C[3] * fs.val[k_off + 1];
        PetscReal _mvv = __x * fs.val[l_off + 0] + __y * fs.val[l_off + 1];
        sum += fs.q.pw[i * 3 + 2] * _mvv * 1;
    }

    return local * sum * 0.5;
}

static inline PetscReal load_vector_2D(PetscReal *invJ,
                                       struct function_space fs, PetscReal detJ,
                                       PetscInt k, PetscInt sign_k)
{
    PetscReal local = PetscAbsReal(detJ) * sign_k;
    PetscReal sum = 0;

    PetscReal f_x = 1.0;
    PetscReal f_y = 1.0;

    // TODO: implement better handling of vector functions
    // TODO: small matrix lib
    for (PetscInt i = 0; i < fs.q.size; i++) {
        int k_off = k * (fs.q.size * 2) + i * 2;
        PetscReal _x =
            invJ[0] * fs.val[k_off + 0] + invJ[2] * fs.val[k_off + 1];
        PetscReal _y =
            invJ[1] * fs.val[k_off + 0] + invJ[3] * fs.val[k_off + 1];
        PetscReal _mvv = _x * f_x + _y * f_y;
        sum += fs.q.pw[i * 3 + 2] * _mvv;
    }

    return local * sum * 0.5;
}

/**
 * Compute invBk * invBk^T when matrix is 2x2
 */
static inline void _invBk_invBkT_2D(PetscReal *invBk, PetscReal *res)
{
    res[0] = invBk[0] * invBk[0] + invBk[1] * invBk[1]; /* a^2 + b^2 */
    res[1] = invBk[0] * invBk[2] + invBk[1] * invBk[3]; /* ac + bd */
    res[2] = res[1];
    res[3] = invBk[2] * invBk[2] + invBk[3] * invBk[3]; /* c^2 + d^2 */
}

#undef __FUNCT__
#define __FUNCT__ "assemble_stiffness"
PetscErrorCode assemble_system(DM dm, struct function_space fs, Mat A, Vec b)
{
    PetscInt dim, cstart, cend, estart, eend, nedges;
    struct ctx *sctx;

    PetscErrorCode ierr = DMGetDimension(dm, &dim);
    CHKERRQ(ierr);
    ierr = DMPlexGetHeightStratum(dm, 0, &cstart, &cend);
    CHKERRQ(ierr);
    ierr = DMPlexGetConeSize(dm, cstart, &nedges);
    CHKERRQ(ierr);
    ierr = DMPlexGetHeightStratum(dm, 1, &estart, &eend);
    CHKERRQ(ierr);
    ierr = DMGetApplicationContext(dm, (void **) &sctx);
    CHKERRQ(ierr);

    PetscLogEventBegin(sctx->matrix_assembly, 0, 0, 0, 0);

    // TODO: napravi nedelec objekt koji je ispod function_space i koji ima
    // assemble_2D i assemble_3D function pointere
    if (dim == 2) {
        PetscReal local[nedges][nedges], load[nedges];
        PetscInt indices[nedges];
        const PetscInt *edgelist;
        for (PetscInt c = cstart; c < cend; c++) {
            PetscInt offset = (c - cstart) * 3;
            PetscReal v0, Bk[4], invBk[4], detBk, _tmp_matrix[4];
            ierr = DMPlexComputeCellGeometryAffineFEM(
                dm, c, &v0, (PetscReal *) &Bk, (PetscReal *) &invBk, &detBk);
            CHKERRQ(ierr);
            ierr = DMPlexGetCone(dm, c, &edgelist);
            CHKERRQ(ierr);

            _invBk_invBkT_2D(invBk, _tmp_matrix);

            for (PetscInt k = 0; k < nedges; k++) {
                PetscInt edge_neighbours, boundary_edge = -1;
                DMPlexGetSupportSize(dm, edgelist[k], &edge_neighbours);
                if (edge_neighbours == 1) {
                    boundary_edge = k;
                }
                PetscInt sign_k = sctx->signs[offset + k];
                indices[k] = edgelist[k] - estart;
                for (PetscInt l = 0; l < nedges; l++) {
                    PetscInt sign_l = sctx->signs[offset + l];

                    if (k == boundary_edge && l == boundary_edge) {
                        local[k][l] = 1.0;
                        load[k] = 0.0;
                    } else if (k == boundary_edge || l == boundary_edge) {
                        local[k][l] = 0.0;
                    } else {
                        local[k][l] = stiffness_matrix_2D(fs, detBk, sign_k,
                                                          sign_l, k, l);
                        local[k][l] += mass_matrix_2D(_tmp_matrix, fs, detBk,
                                                      sign_k, sign_l, k, l);
                    }
                }
                if (k != boundary_edge) {
                    load[k] = load_vector_2D(invBk, fs, detBk, k, sign_k);
                }
            }
            ierr = MatSetValues(A, nedges, indices, nedges, indices,
                                *local, ADD_VALUES);
            CHKERRQ(ierr);
            ierr = VecSetValues(b, nedges, indices, load, ADD_VALUES);
            CHKERRQ(ierr);
        }
    } else {
        SETERRQ(PETSC_COMM_SELF, 56, "3D is currently not supported\n");
    }

    // TODO: pogledaj u dokumentaciji
    MatAssemblyBegin(A, MAT_FINAL_ASSEMBLY);
    MatAssemblyEnd(A, MAT_FINAL_ASSEMBLY);
    VecAssemblyBegin(b);
    VecAssemblyEnd(b);

    PetscLogEventEnd(sctx->matrix_assembly, 0, 0, 0, 0);

    return (0);
}
