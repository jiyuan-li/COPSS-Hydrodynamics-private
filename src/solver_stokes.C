// Parallel Finite Element-General Geometry Ewald-like Method.

// Copyright (C) 2015-2016 Xujun Zhao, Jiyuan Li, Xikai Jiang

// This code is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.


// This code is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.


// You should have received a copy of the GNU General Public
// License along with this code; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA


#include "solver_stokes.h"

// make sure libMesh has been complied with PETSc
#ifdef LIBMESH_HAVE_PETSC

// include PETSc KSP solver
EXTERN_C_FOR_PETSC_BEGIN
# include <petscsys.h>
# include <petscksp.h>
# include <petscpc.h>
# include <petscis.h>
# include <petscviewer.h>
EXTERN_C_FOR_PETSC_END

// libmesh headers
# include "libmesh/libmesh_config.h"
# include "libmesh/dof_map.h"
# include "libmesh/system.h"
# include "libmesh/linear_implicit_system.h"
# include "libmesh/petsc_matrix.h"
# include "libmesh/petsc_vector.h"
# include "libmesh/petsc_linear_solver.h"

// local header
# include "pm_system_stokes.h"

using namespace libMesh;


// =======================================================================================
SolverStokes::SolverStokes(EquationSystems& es)
  : Solver(es)
{}

// =======================================================================================
SolverStokes::SolverStokes(EquationSystems      & es,
                           const SystemSolverType solver_type)
  : Solver(es, solver_type)
{}

// =======================================================================================
SolverStokes::~SolverStokes()
{
  // destroy the PETSc context if it is created in this function.
  // NOTE: if init_ksp_solver() is not called, We don't destroy!
  if (_is_init)
  {
    int ierr;

    if (_solver_type == field_split)
    {
      ierr = MatDestroy(&_schur_pmat);   CHKERRABORT(this->comm().get(), ierr);
      ierr = ISDestroy(&_is_v);          CHKERRABORT(this->comm().get(), ierr);
      ierr = ISDestroy(&_is_p);          CHKERRABORT(this->comm().get(), ierr);
    }
    ierr = KSPDestroy(&_ksp);   CHKERRABORT(this->comm().get(), ierr);
  }
}

// ==================================================================================
void SolverStokes::init_ksp_solver(const std::string& system_name)
{
  START_LOG("init_ksp_solver()", "SolverStokes");

  // Collect control parameters of KSP solver.(Don't put these in the
  // constructor!)
  unsigned int max_iter = _equation_systems.parameters.get<unsigned int>(
    "linear solver maximum iterations");
  Real linear_sol_rtol = _equation_systems.parameters.get<Real>(
    "linear solver rtol");
  Real linear_sol_atol = _equation_systems.parameters.get<Real>(
    "linear solver atol");
  _rtol   = static_cast<PetscReal>(linear_sol_rtol);
  _atol   = static_cast<PetscReal>(linear_sol_atol);
  _max_it = static_cast<PetscInt>(max_iter);

  // Get a reference to the system Matrix
  PMSystemStokes& system = _equation_systems.get_system<PMSystemStokes>
    (system_name);
  PetscMatrix<Number> *matrix = cast_ptr<PetscMatrix<Number> *>(system.matrix);

  // this->petsc_view_matrix( matrix->mat() );

  // set up KSP in PETSc, resuse the Preconditioner
  // PetscPrintf(this->comm().get(), "--->test in
  // SolverStokes::init_ksp_solver(): ");
  // PetscPrintf(this->comm().get(), "Start to initialize the PETSc KSP
  // solver...\n");

  int ierr;
  ierr = KSPCreate(this->comm().get(), &_ksp);               CHKERRABORT(
    this->comm().get(),
    ierr);
  ierr = KSPSetOperators(_ksp, matrix->mat(), matrix->mat()); CHKERRABORT(
    this->comm().get(),
    ierr);
  ierr = KSPSetReusePreconditioner(_ksp, PETSC_TRUE);         CHKERRABORT(
    this->comm().get(),
    ierr);
  ierr = KSPSetTolerances(_ksp, _rtol, _atol, PETSC_DEFAULT, _max_it);

  // ierr = KSPSetTolerances (_ksp, _rtol, PETSC_DEFAULT, PETSC_DEFAULT,
  // max_its);
  CHKERRABORT(this->comm().get(), ierr);
  ierr = KSPSetFromOptions(_ksp);                            CHKERRABORT(
    this->comm().get(),
    ierr);

  // ierr = KSPSetUp(_ksp); CHKERRABORT(this->comm().get(), ierr); // cause
  // error

  // start from non-zero inital guess can reduce the total iteration steps of
  // convergence?
  // actually NOT at all, and sometimes even worse !
  // ierr = KSPSetInitialGuessNonzero(_ksp,PETSC_TRUE);
  // CHKERRABORT(this->comm().get(),ierr);

  // Collect Index Set for velocity and pressure;
  // Retrieve the approximate Schur Complement S = -(Mp + C);
  // Set up sub-user-KSP and sub-user-PC for solving Schur S*y1 = y2.
  if (_solver_type == field_split)
  {
    this->build_is(&_is_v, &_is_p);

    // this->petsc_view_is(_is_v);

    // compute the approximate schur complement S
    this->setup_approx_schur_matrix(_is_v, _is_p, &_schur_pmat);

    // this->petsc_view_matrix(_schur_pmat);

    // set up PC for schur complement
    const bool userPC  = true;
    const bool userKSP =
      _equation_systems.parameters.get<bool>("schur_user_ksp");
    this->setup_schur_pc(_ksp, _is_v, _is_p, &_schur_pmat, userPC, userKSP);
  }

  // label the solver
  _is_init = true;

  // PetscPrintf(this->comm().get(), "--->test in
  // SolverStokes::init_ksp_solver(): ");
  // PetscPrintf(this->comm().get(), "the PETSc KSP solver is initialized \n");
  STOP_LOG("init_ksp_solver()", "SolverStokes");
}

// =======================================================================================
void SolverStokes::build_is(IS *is_v,
                            IS *is_p)
{
  START_LOG("build_is()", "SolverStokes");

  int ierr;
  const MeshBase& mesh         = _equation_systems.get_mesh();
  const unsigned int dim       = mesh.mesh_dimension();
  LinearImplicitSystem& system =
    _equation_systems.get_system<LinearImplicitSystem>("Stokes");

  // collect index for u and p
  std::vector<dof_id_type> indices;

  for (unsigned int v = 0; v < dim; ++v)
  {
    std::vector<dof_id_type> var_idv;
    system.get_dof_map().local_variable_indices(var_idv, mesh, v);
    indices.insert(indices.end(), var_idv.begin(), var_idv.end());
  }

  std::vector<dof_id_type> var_idp;
  const unsigned int p_var = system.variable_number("p");
  system.get_dof_map().local_variable_indices(var_idp, mesh, p_var);

  // collect indices and create the IS for u and p
  const PetscInt *index_v = PETSC_NULL;
  const PetscInt *index_p = PETSC_NULL;

  if (!indices.empty()) {
    index_v = reinterpret_cast<const PetscInt *>(&indices[0]);
  }

  if (!var_idp.empty()) {
    index_p = reinterpret_cast<const PetscInt *>(&var_idp[0]);
  }

  ierr = ISCreateLibMesh(this->comm().get(),
                         indices.size(),
                         index_v,
                         PETSC_COPY_VALUES,
                         is_v);
  CHKERRABORT(this->comm().get(), ierr);
  ierr = ISCreateLibMesh(this->comm().get(),
                         var_idp.size(),
                         index_p,
                         PETSC_COPY_VALUES,
                         is_p);
  CHKERRABORT(this->comm().get(), ierr);

  // sort IS
  ierr = ISSort(*is_v);  CHKERRABORT(this->comm().get(), ierr);
  ierr = ISSort(*is_p);  CHKERRABORT(this->comm().get(), ierr);

  // output and log stage
  ierr = PetscPrintf(this->comm().get(),
                     "SolverStokes: Indices Sets are built! \n");
  CHKERRABORT(this->comm().get(), ierr);

  STOP_LOG("build_is()", "SolverStokes");
}

// =======================================================================================
void SolverStokes::setup_schur_pc(KSP        ksp,
                                  IS         is_v,
                                  IS         is_p,
                                  Mat       *pmat, /* the schur complement
                                                      preconditioning matrix */
                                  const bool userPC,
                                  const bool userKSP)
{
  START_LOG("setup_schur_pc()", "SolverStokes");

  PC  pc;
  int ierr;

  // set up PC for KSP
  ierr = KSPGetPC(ksp, &pc);               CHKERRABORT(this->comm().get(), ierr);
  ierr = PCFieldSplitSetIS(pc, "0", is_v); CHKERRABORT(this->comm().get(), ierr);
  ierr = PCFieldSplitSetIS(pc, "1", is_p); CHKERRABORT(this->comm().get(), ierr);

  // set up the user-defined PC for the schur complement
  if (userPC)
  {
    ierr = PCFieldSplitSetSchurPre(pc, PC_FIELDSPLIT_SCHUR_PRE_USER, *pmat);
    CHKERRABORT(this->comm().get(), ierr);
  }

  // set up the sub-KSP
  if (userKSP)
  {
    PetscInt  n_split = 1; /* the number of split */
    PetscInt  max_its = 100;
    PetscReal rtol    = _equation_systems.parameters.get<Real>(
      "schur_user_ksp_rtol");
    PetscReal atol = _equation_systems.parameters.get<Real>(
      "schur_user_ksp_atol");
    KSP *subksp;
    ierr = PCSetUp(pc);                               CHKERRABORT(
      this->comm().get(),
      ierr);
    ierr = PCFieldSplitGetSubKSP(pc, &n_split, &subksp); CHKERRABORT(
      this->comm().get(),
      ierr);
    ierr = KSPSetOperators(subksp[1], *pmat, *pmat);  CHKERRABORT(
      this->comm().get(),
      ierr);
    ierr = KSPSetTolerances(subksp[1], rtol, atol, PETSC_DEFAULT, max_its);
    CHKERRABORT(this->comm().get(), ierr);
    ierr = KSPSetFromOptions(subksp[1]);              CHKERRABORT(
      this->comm().get(),
      ierr);
    ierr = PetscFree(subksp);                         CHKERRABORT(
      this->comm().get(),
      ierr);
  }

  // end of function
  ierr = PetscPrintf(this->comm().get(),
                     "SolverStokes: PC for Schur complement is set up! \n");
  CHKERRABORT(this->comm().get(), ierr);

  STOP_LOG("setup_schur_pc()", "SolverStokes");
}

// =======================================================================================
void SolverStokes::setup_approx_schur_matrix(IS   is_v,
                                             IS   is_p,
                                             Mat *pmat)
{
  START_LOG("setup_approx_schur_matrix()", "SolverStokes");

  // get the Stokes system
  int ierr;
  LinearImplicitSystem& system =
    _equation_systems.get_system<LinearImplicitSystem>("Stokes");
  const std::string schur_pc_type =
    _equation_systems.parameters.get<std::string>("schur_pc_type");

  if ((schur_pc_type == "SMp") || (schur_pc_type == "S3") ||
      (schur_pc_type == "SMp_lump"))
  {
    // get the pointer to the PC matrix assembled in the stokes_assemble()
    PetscMatrix<Number> *pc_matrix;
    pc_matrix =
      cast_ptr<PetscMatrix<Number> *>(system.request_matrix("Preconditioner"));
    PetscMatrix<Number> *sys_matrix = cast_ptr<PetscMatrix<Number> *>(
      system.matrix);

    // We extract the submatrix directly using PETSc function,
    // because libMesh function use std::vector argument for is, does not
    // directly use IS,
    Mat Kpp;
    ierr =
      MatGetSubMatrix(sys_matrix->mat(), is_p, is_p, MAT_INITIAL_MATRIX, &Kpp);
    ierr =
      MatGetSubMatrix(pc_matrix->mat(),  is_p, is_p, MAT_INITIAL_MATRIX, pmat);
    ierr = MatAXPY(*pmat, 1.0, Kpp, DIFFERENT_NONZERO_PATTERN); // pmat =
                                                                // 1.0*Kpp +
                                                                // pmat
    // ierr = MatScale(*pmat,-1.0);
    ierr = MatDestroy(&Kpp);            CHKERRABORT(this->comm().get(), ierr);
  }
  else if ((schur_pc_type == "S1") || (schur_pc_type == "S2"))
  {
    // get the pointer to the global preconditioner matrix assembled in the
    // stokes_assemble()
    // NOTE, we don't use global system matrix and rhs, because submatrix will
    // overwrite them.
    // PetscMatrix<Number>* sys_matrix = cast_ptr<PetscMatrix<Number>*>(
    // system.matrix );
    // PetscVector<Number>* sys_rhs    = cast_ptr<PetscVector<Number>*>(
    // system.rhs );

    PetscMatrix<Number> *pc_matrix;
    pc_matrix =
      cast_ptr<PetscMatrix<Number> *>(system.request_matrix("Preconditioner"));
    NumericVector<Number>& sol_in  = *(system.solution);
    PetscVector<Number>   *sys_rhs = cast_ptr<PetscVector<Number> *>(&sol_in);

    // extract the submatrices K11(Kuu) and K12(Kup)
    Mat Kuu, Kup, Kpu, Kpp;
    ierr =
      MatGetSubMatrix(pc_matrix->mat(), is_v, is_v, MAT_INITIAL_MATRIX, &Kuu);
    ierr =
      MatGetSubMatrix(pc_matrix->mat(), is_v, is_p, MAT_INITIAL_MATRIX, &Kup);
    ierr =
      MatGetSubMatrix(pc_matrix->mat(), is_p, is_v, MAT_INITIAL_MATRIX, &Kpu);
    ierr =
      MatGetSubMatrix(pc_matrix->mat(), is_p, is_p, MAT_INITIAL_MATRIX, &Kpp);
    CHKERRABORT(this->comm().get(), ierr);

    // we test compute Kpu = transpose(Kup), which gives the same results as
    // submatrix
    // ierr = MatTranspose(Kup,MAT_INITIAL_MATRIX,&Kpu);
    // CHKERRABORT(this->comm().get(), ierr);

    // inverse of diagonal of Kuu
    // it is necessary to use subvec to maintain conforming mat and vec sizes!
    Vec v_vec;
    ierr = VecGetSubVector(sys_rhs->vec(), is_v, &v_vec); CHKERRABORT(
      this->comm().get(),
      ierr);
    ierr = MatGetDiagonal(Kuu, v_vec);  CHKERRABORT(this->comm().get(), ierr);
    ierr = VecReciprocal(v_vec);        CHKERRABORT(this->comm().get(), ierr);
    ierr = VecScale(v_vec, -1.0);        CHKERRABORT(this->comm().get(), ierr);
    ierr = MatDestroy(&Kuu);            CHKERRABORT(this->comm().get(), ierr);

    // this->petsc_view_vector(v_vec);
    // this->petsc_view_matrix(Kup);    this->petsc_view_matrix(Kpu);

    // compute: - [ Kpu diag(Kuu)^(-1) Kup + C (-Kpp) ]
    ierr = MatDiagonalScale(Kup, v_vec, NULL); // left scale Kup (*warning*
                                               // overwrites Kup)
    ierr = MatMatMult(Kpu, Kup, MAT_INITIAL_MATRIX, PETSC_DEFAULT, pmat);
    ierr = MatAXPY(*pmat, 1.0, Kpp, DIFFERENT_NONZERO_PATTERN);

    // ierr = MatScale(*pmat,-1.0);  // "-" leads to more iterations to
    // converge!
    CHKERRABORT(this->comm().get(), ierr);

    // destroy the PETSc context
    ierr = VecDestroy(&v_vec);          CHKERRABORT(this->comm().get(), ierr);
    ierr = MatDestroy(&Kup);            CHKERRABORT(this->comm().get(), ierr);
    ierr = MatDestroy(&Kpu);            CHKERRABORT(this->comm().get(), ierr);
    ierr = MatDestroy(&Kpp);            CHKERRABORT(this->comm().get(), ierr);

    // extract the diagonal values of p for S2 type preconditioner
    if (schur_pc_type == "S2")
    {
      Vec p_vec;
      ierr = VecGetSubVector(sys_rhs->vec(), is_p, &p_vec); CHKERRABORT(
        this->comm().get(),
        ierr);
      ierr = MatGetDiagonal(*pmat, p_vec);               CHKERRABORT(
        this->comm().get(),
        ierr);
      ierr = MatZeroEntries(*pmat);                       CHKERRABORT(
        this->comm().get(),
        ierr);
      ierr = MatSetOption(*pmat, MAT_IGNORE_ZERO_ENTRIES, PETSC_TRUE);
      ierr = MatDiagonalSet(*pmat, p_vec, INSERT_VALUES);   CHKERRABORT(
        this->comm().get(),
        ierr);
      ierr = VecDestroy(&p_vec);                          CHKERRABORT(
        this->comm().get(),
        ierr);
    }
  } // end if-else if

  ierr = PetscPrintf(this->comm().get(),
                     "SolverStokes: Approximate Schur matrix is computed!\n");
  CHKERRABORT(this->comm().get(), ierr);

  STOP_LOG("setup_approx_schur_matrix()", "SolverStokes");
}

#endif // ifdef LIBMESH_HAVE_PETSC
