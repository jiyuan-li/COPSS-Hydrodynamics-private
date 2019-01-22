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

// Local Includes -----------------------------------
#include "assemble_poisson.h"
#include "solver_stokes.h"
#include "pm_linear_implicit_system.h"

namespace libMesh
{

/*
 * The PMSystemPoisson is designed to solve the Poisson
 * equation
 */

class PMSystemPoisson : public PMLinearImplicitSystem
{
public:

  /**
   * Constructor.
   */
  PMSystemPoisson (EquationSystems& es,
                    const std::string& name,
                    const unsigned int number); // number of systems


  /**
   * Destructor.
   */
  virtual ~PMSystemPoisson ();


  /**
   * The type of system.
   */
  typedef PMSystemPoisson sys_type;


  /**
   * @returns a clever pointer to the system.
   */
  sys_type & system () { return *this; }


  /**
   * Clear all the data structures associated with the system.
   */
  void clear ();


  /**
   * Assemble the system matrix.
   * option ==
   */
  void assemble_matrix (const std::string& system_name,
                        const std::string& option);


  /**
   * Assemble the system rhs.
   */
  void assemble_rhs (const std::string& system_name,
                     const std::string& option);


  /*
   * Solve the system.
   * option = ...
   * re_init = true => re-assemble the matrix and reinit the KSP solver.
   */
  void solve (const std::string& option,
              const bool& re_init);
              
  // /*
  //  * Return the NPSolver
  //  */
  SolverStokes& poisson_solver() { return _poisson_solver;  }



private:

  // // Stokes solver
  SolverStokes _poisson_solver;

  // Assemble Stokes system
  AssemblePoisson* _assemble_poisson;

};

} // end namespace libMesh