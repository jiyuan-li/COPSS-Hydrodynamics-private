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

// libmesh Includes -----------------------------------
#include "libmesh/libmesh_config.h"
#include "libmesh/petsc_macro.h"
#include "libmesh/linear_implicit_system.h"

// Local Includes -----------------------------------
#include "point_mesh.h"
#include "particle_mesh.h"

// C++ Includes -------------------------------------
#include <stdio.h>
#include <cstddef>


namespace libMesh {
/*
 * The PMLinearImplicitSystem is designed to provide
 * a basic class for solving partial different equations
 * with regularized Gaussian point forces due to particles
 * using mixed FEM.
 */

class Fix;


class PMLinearImplicitSystem : public LinearImplicitSystem {
public:

  /**
   * Constructor.
   */
  PMLinearImplicitSystem(EquationSystems  & es,
                         const std::string& name,
                         const unsigned int number); // number of systems


  /**
   * Destructor.
   */
  virtual ~PMLinearImplicitSystem();


  /**
   * The type of system.
   */
  typedef PMLinearImplicitSystem sys_type;


  /**
   * @returns a clever pointer to the system.
   */
  sys_type& system() {
    return *this;
  }

  /**
   * Clear all the data structures associated with the parent system.
   */
  virtual void clear() = 0;

  /**
   * get the time interval to guarantee numerical stability for a system
   */
  virtual Real get_dt() {return std::numeric_limits<double>::max();};

  /**
   * Assemble the system matrix.
   */
  virtual void assemble_matrix(const std::string& system_name,
                               const std::string& option) = 0;


  /**
   * Assemble the system rhs.
   */
//  virtual void assemble_rhs(const std::string& system_name,
//                            const std::string& option);


  /**
   * Solve the system.
   */
//  virtual void solve(const std::string& option) = 0;


  /**
   * Update solution to total solution, this function will also backup
   * solution to global solution
   */
  virtual void update_solution_to_total() = 0;

  /**
   * Resume solution to global solution of the system. This function often
   * gets called after update_solution_to_total()
   */
  virtual void resume_solution_to_global() = 0;


    /**
   * Compute the L2-error by comparing numerical and analytical solutions
   */
  void test_nodal_error(bool& neighbor_list_update_flag);


  /*
   * Attach PointMesh
   */
  void attach_point_mesh(PointMesh<3> *pm) {
    _point_mesh = pm;
  }

  /*
   * return editable PointMesh ptr and const PointMesh ptr
   */
  PointMesh<3>* point_mesh() {
    return _point_mesh;
  }

  PointMesh<3>* point_mesh() const {
    return _point_mesh;
  }

  /*
   * Attach ParticleMesh
   */
  void attach_particle_mesh(ParticleMesh<3> *pm) {
    _particle_mesh = pm;
  }

  /*
   * return editable ParticleMesh ptr and const ParticleMesh ptr
   */
  ParticleMesh<3>* particle_mesh() {
    return _particle_mesh;
  }

  ParticleMesh<3>* particle_mesh() const {
    return _particle_mesh;
  }

  /*
   * Attach the ForceField
   */
  void attach_fixes(std::vector<Fix *>fixes) {
    _fixes = fixes;
  }

  std::vector<Fix *>fixes() {
    return _fixes;
  }

  std::vector<Fix *>fixes() const {
    return _fixes;
  }

  /**
   * write out single particle(point particle) info, including:
   * timestep, time, particle_xyz,  particle_velocity
   */
  void write_out_single_particle(const Point            & coords,
                                 const std::vector<Real>& vel,
                                 const int                i_step,
                                 const Real               time) const;


  /**
   * Write out particle(point) coordinates represented by a PETSc vector
   */
  void write_out_point_coordinate(Vec               *ROUT,
                                  const std::size_t  istep,
                                  const Real       & time,
                                  const std::string& f_name,
                                  const std::string& openmode) const;


  /**
   * Write out particle(point) coordinates
   * The output format is Comma Separated Variable(CSV) for ParaView.
   */
  void write_point_csv(const std::string      & filename,
                       const std::vector<Real>& pv,
                       const bool               write_velocity) const;

  PetscErrorCode write_point_csv(const std::string& filename,
                                 Vec               *petsc_vector,
                                 const bool         write_velocity) const;


protected:

  // particle mesh pointer
  PointMesh<3> *_point_mesh = nullptr;

  // particle mesh pointer
  ParticleMesh<3> *_particle_mesh = nullptr;

  // force field for particle/points
  std::vector<Fix *>_fixes;
  
  // bool _re_init: If true, global_matrix will be rebuilt. It's only true
  // at the beginining of the simulation.
  bool _re_init;

  // A Pointer to the global_solution
  UniquePtr<NumericVector<Number>> _global_solution;

};
} // end namespace libMesh
