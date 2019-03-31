#ifndef NEDELEC_H
#define NEDELEC_H

#include "quadrature.h"

struct function_space {
    PetscScalar *val;
    PetscScalar *cval;
    PetscInt nbasis;
    PetscErrorCode (*assemble_matrix)(struct function_space *,
                                      struct quadrature, Mat, DM);
};

PetscErrorCode nedelec_basis(struct quadrature q, struct function_space *fspace);

#endif /* NEDELEC_H */