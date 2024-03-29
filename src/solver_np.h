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


#pragma once

// Local includes
#include "solver.h"


/*
 * This class solves Nernst Planck equation using either an iterative
 * solver with a preconditioner or direct solver (for example, superLU)
 * using PETSc interface.
 */
class SolverNP : public Solver {
public:

  /*
   * Constructor
   */
  SolverNP(EquationSystems& es);


  /*
   * Constructor
   */
  SolverNP(EquationSystems      & es,
          const SystemSolverType solver_type);


  /*
   * Destructor
   */
  ~SolverNP();


  /*
   * Init the KSP solver:
   * The system matrix needs to be assembled before calling this init function!
   */
  void init_ksp_solver(const std::string& system_name);
};
