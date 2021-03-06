#include <petsc.h>

#include "nedelec.h"
#include "quadrature.h"

/**
 * M represents number of quadrature points
 */
PetscErrorCode nedelec_basis(struct function_space *fspace, int q_order)
{
    generate_quad(&fspace->q, q_order);

    PetscInt M = fspace->q.size;

    /* three different M x 2 matrix */
    PetscErrorCode ierr = PetscMalloc1(3 * M * 2, &fspace->val);
    CHKERRQ(ierr);
    ierr = PetscMalloc1(3 * M * 1, &fspace->cval);
    CHKERRQ(ierr);

    for (PetscInt i = 0; i < M; i++) {
        int _3i = 3 * i;
        int offset_0 = 0 * (M * 2) + i * 2;
        int offset_1 = 1 * (M * 2) + i * 2;
        int offset_2 = 2 * (M * 2) + i * 2;

        // zero basis function for i-th quadrature point (x, y)
        fspace->val[offset_0 + 0] = -fspace->q.pw[_3i + 1];
        fspace->val[offset_0 + 1] = fspace->q.pw[_3i + 0];

        fspace->val[offset_1 + 0] = -fspace->q.pw[_3i + 1];
        fspace->val[offset_1 + 1] = fspace->q.pw[_3i + 0] - 1;

        fspace->val[offset_2 + 0] = 1 - fspace->q.pw[_3i + 1];
        fspace->val[offset_2 + 1] = fspace->q.pw[_3i + 0];

        fspace->cval[_3i + 0] = 2;
        fspace->cval[_3i + 1] = 2;
        fspace->cval[_3i + 2] = 2;
    }

    fspace->nbasis = 3;

    return ierr;
}
