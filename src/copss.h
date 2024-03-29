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

// C++ includes
#include <iostream>
#include <algorithm>
#include <cstring>
#include <math.h>
#include <time.h>
#include <fstream>
#include <tuple>
#include <stdlib.h>

// Libmesh includes
#include "libmesh/libmesh.h"
#include "libmesh/libmesh_config.h"
#include "libmesh/getpot.h"
#include "libmesh/exodusII_io.h"
#include "libmesh/enum_solver_package.h"

#include "libmesh/serial_mesh.h"
#include "libmesh/mesh_generation.h"
#include "libmesh/mesh_modification.h"
#include "libmesh/mesh_refinement.h"

#include "libmesh/dof_map.h"
#include "libmesh/linear_solver.h"
#include "libmesh/equation_systems.h"
#include "libmesh/linear_implicit_system.h"
#include "libmesh/periodic_boundary.h"
#include "libmesh/sparse_matrix.h"
#include "libmesh/numeric_vector.h"
#include "libmesh/slepc_macro.h"
#include "libmesh/perf_log.h"

// User defined includes
#include "point_particle.h"
#include "particle_mesh.h"
#include "point_mesh.h"
#include "pm_system_stokes.h"
#include "pm_system_poisson.h"
#include "pm_system_np.h"
#include "brownian_system.h"
#include "pm_periodic_boundary.h"
#include "chebyshev.h"
#include "pm_toolbox.h"
#include "polymer_chain.h"
#include "random_generator.h"
#include "solver.h"
#include "copss_init.h"
#include "fix/fix_factory.h"
#include "fix/fix.h"


/*! This file serves as a template to build\solve a Stokes system
    using our pFE-GgEm code
 */

// Bring in everything from the libMesh namespace


namespace libMesh {
class Copss {
public:

  // PETSC MPI communicator
  Parallel::Communicator *comm_in = nullptr;

  // error message string
  std::string error_msg;

  // output message string
  std::string output_msg;
  
  std::ostringstream ss;

  // control file object
  GetPot input_file;

  // control file name
  std::string control_fileName;

  // test name
  std::string simulation_name;
  bool print_info;

  // modules
  bool module_poisson;
  bool module_np;

  // physical parameters
  const Real kB                = 1.380662E-17;         // 1.380662E-23(J/K) =
                                                       // 1.3806623E-23(N*m/K) =
                                                       // 1.380662E-17(N*um/K)
  const Real PI                = libMesh::pi;
  const Real epsilon_0         = 8.85418781762039E-24; // 8.85418781762039E-12(F/m)
                                                       // = 8.85418781762039E-12(
                                                       // C^2/(N*m^2) ) =
                                                       // 8.85418781762039E-24 (
                                                       // C^2/(N*um^2) )
  const Real elementary_charge = 1.6021766208E-19;     // unit = C
  const Real NA = 6.02214076E23;      // Avogadro constant, unit = [1/mol]
  Real T;                                              // simulation temperature
                                                       // (K)
  Real kBT;                                            // (N*um)
  Real viscosity;                                      // viscosity of the fluid
                                                       // (cp = N*s/um^2)
  Real epsilon;                                        // relative permittivity
                                                       // of the fluid
  Real Rb;                                             // radius of the bead
  Real drag_c;                                         // Drag coefficient
                                                       // (N*s/um)
  Real Db; // bead diffusivity (um^2/s)
  std::string particle_type;
  // name of each ion species
  std::vector<std::string> ion_name;
  // diffusivity of each ion species (unit = um^2/s)
  std::vector<Real> ion_diffusivity;
  // valence of each ion species (unit = 1)
  std::vector<int> ion_valence;
  // initial bulk ion concentration normalized by c0 (unit=1)
  std::vector<Real> ion_concentration_bulk;
  // Bjerrum length normalized by Rb, unit = 1
  Real lambda_B;
  // Debye length normalized by Rb, unit = 1
  Real lambda_D;
  // NP system relaxation time for initialization. By default, this
  // relaxation time is 2 * Rb^2/max(ion_diffusivity), i.e., the time it takes
  // for ions to diffuse 2 particle radius. 
  Real np_system_relaxation_time;
  // np system relaxation write interval
  unsigned int np_system_relaxation_write_interval;
  // dt for np system
  Real np_dt;

  // characteristic variables
  Real tc; // characteristic time (diffusion time) (s)
  Real uc; // characteristic velocity (um/s)
  Real fc; // characteristic force (N)
  Real muc; // non-dimension viscosity
  Real phi0; // characteristic electristatic potential (V)
  Real efield0; // characteristic electric field (V/um)
  Real charge_rho0; // characteristic volume charge density (C/um^3)
  Real charge_sigma0; // characteristic surface charge density (C/um^2)
  Real c0; // characteristic concentration of ion species (M=mol/L)

  // coefficient used when calculating the contribution of ion cloud to global
  // electrostatic potential, unit = [1]. Contribution of ions to global
  // charge density is defined as Sum{NA * Zj * e * Cj} where j is the j-th
  // ion species, Cj is the concentration which has the unit Mol/(10^-1 m)^3.
  // In dimensionless Poisson equation, the terms shrinks to Rb^3 / (10^-1 m)
  // ^3 * NA * Sum{zj * Cj}, i.e., thus we have this coefficient NA * Rb^3 /
  // (10^5 m)^3
  Real coeff_ion_charge_density;

  // coefficient used when calculating the contribution of ion cloud to
  // global force density, unit = [1]. Contribution of ions to global force
  // density is defined as Sum{NA * Zj * e * E}, where j is the j-th ion
  // species, Cj is the concentration which has the unit Mol/(10^-1 m)^3. In
  // dimensionless Stokes equation, this term shrinks to  (e * NA * mol /
  // (10^-1 m)^3) * (e / (4 * pi * epsilon_0 * epsilon * Rb * Rb)) / (fc / Rb^3)
  Real coeff_ion_force_density;

  // Geometry information
  unsigned int dim;                         // dimension of the box
  std::string wall_type;                    // wall_type (slit or sphere)
  std::vector<Real>wall_params;             // (wall parameters; e.g. slit =
                                            // '-50,50,-50,50,-50,50' or sphere
                                            // = '10')
  std::vector<bool>periodicity;             // periodicity of the box
  std::vector<bool>inlet;                   // inlet direction of the box
  std::vector<Real>inlet_pressure;          // inlet pressure
  std::vector<bool>shear;                   // apply shear or not on boundary
                                            // pairs
  std::vector<Real>shear_rate;              // shear rate on boundary pairs
  std::vector<unsigned int>shear_direction; // shear direction on boundary pairs

  // Mesh information
  bool generate_mesh;                       // flag to generate mesh or load
                                            // mesh
  std::string domain_mesh_file;             // domain mesh filename
  std::vector<unsigned int>n_mesh;          // mesh size in all directions

  // Boundary conditions
  std::vector<unsigned int>boundary_id_dirichlet_poisson,
                           boundary_id_neumann_poisson;
  std::vector<Real>boundary_value_dirichlet_poisson,
                   boundary_value_neumann_poisson;

  // Boundary conditions for Nernst-Planck system; notice that the ion
  // concentration provided in the input file should be the normalized value
  // by c0
  bool check_charge_neutrality;
  std::vector<unsigned int> boundary_id_dirichlet_np;
  std::vector<std::vector<Real> > boundary_value_dirichlet_np;

  // Fix
  std::vector<Fix *>fixes;
  FixFactory *fix_factory = nullptr;
  unsigned int numForceTypes;
  std::vector<std::string>forceTypes;
  std::vector<Fix::type_force>forces;

  // map force_type to Fix Pointers


  // GGEM information
  Real alpha;

  // Solver information
  int max_linear_iterations;
  Real linear_solver_rtol;
  Real linear_solver_atol;
  bool user_defined_pc, user_defined_pc_poisson, user_defined_pc_np;
  bool schur_user_ksp;
  Real schur_user_ksp_rtol;
  Real schur_user_ksp_atol;
  std::string schur_pc_type;
  std::string solver_stokes, solver_poisson, solver_np;
  SystemSolverType solver_type_stokes, solver_type_poisson, solver_type_np;

  // Chebyshev information
  unsigned int max_n_cheb;       // max order of Chebyshev polynomianl
  Real tol_cheb;                 // tolerance of chebyshev convergence
  Real eig_factor;               // factor to resize eigen value range
  Real tol_eigen;                // tolerance of eigenvalue convergence
  bool compute_eigen;            // if compute eigen value; false by default
  bool read_eigen;               // if read eigen value from existed file; false
                                 // by default
  // Run time info
  bool with_hi;
  bool with_brownian;            // if consider brownian motion
  std::size_t random_seed;
  bool adaptive_dt;              // if use adaptive time step (essential for
                                 // brownian systems)
  std::vector<Real>max_dr_coeff; // max displacement per step
  Real max_dr;
  bool restart;                  // if restart
  std::size_t restart_step;      // restart step
  unsigned int nstep;            // totol number of steps to run
  unsigned int n_relax_step;     // relaxation steps when Chebyshev cannot
                                 // converge even after recompute eigenvalue
  bool debug_info;

  // ouput file
  unsigned int write_interval; // output file write interval
  std::vector<std::string>output_file;
  std::string solution_name;

  // mesh
  SerialMesh *mesh = nullptr;
  PointMesh<3> *point_mesh = nullptr;

  // std::unique_ptr<SerialMesh> mesh;
  Real min_mesh_size, max_mesh_size;
  Real search_radius_p, search_radius_e;

  // periodic boundary
  PMPeriodicBoundary *pm_periodic_boundary = nullptr;

  // std::unique_ptr<PMPeriodicBoundary> pm_periodic_boundary;

  // equation system
  unsigned int u_var, v_var, w_var, p_var, phi_var, c_var;

  // integrate
  // paramters for dynamic process;
  bool reinit_stokes;
  unsigned int NP;
  unsigned int n_vec;
  Real hminf, hmaxf; // fluid mesh minimum
  Real hmin, hmax;   // all mesh minimum
  bool cheb_converge = false;
  unsigned int n_chebyshev_failure = 0;
  Real eig_min = 0, eig_max = 0;

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    -
     Create vectors and Shell Mat for use:
     U0:          particle velocity vector;
     R0/R_mid:    particle position vector;
     dw/dw_mid:   random vector;
     RIN/ROUT:    the initial and intermediate particle postion vector for msd
       output
     RIN will not change, and ROUT excludes pbc
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
       - */
  Vec U0, R0, R_mid, RIN, ROUT, dw, dw_mid;
  PetscViewer viewer;
  Mat M;
  PetscRandom rand_ctx;
  PetscScalar coef = 0.0;
  BrownianSystem *brownian_sys = nullptr;
  std::vector<Point>center0;

  // variables in integrator
  unsigned int istart;
  unsigned int o_step;
  Real real_time;                                 // real time
  bool update_neighbor_list_everyStep;
  unsigned int neighbor_list_update_interval = 0; // how often to update
                                                  // neighbor list
  unsigned int timestep_duration;                 // how many steps elapsed
                                                  // since last neighbor_list
                                                  // update
  bool neighbor_list_update_flag;
  std::vector<Real>vel0;
  std::vector<Real>vel1;


  /*!
   * class constructor
   */
  Copss(CopssInit& init);

  /*!
   * class destructor
   */
  virtual ~Copss();

  /*! Check libmesh support
   *
   * PETSC or laspack
   * SLEPC
   * AMR
   * at least 2D
   */
  int             check_libmesh();

  /*!
   * Print out start time
   */
  void            start_time(struct tm *timeinfo);

  /*!
   * Print out end time
   */
  void            end_time(struct tm *timeinfo);

  /*!
   * Build and initialized an Equation system
   * step 1: read_input()
   * step 2: create_object_mesh()
   * step 3: create_equation_systems()
   */
  EquationSystems init_system(std::string input_file); // including the
                                                       // following 10 steps

  /*!
   * Read input and create necessary objects
   * step 1: read_system_info()
   * step 2: read_physical_info()
   * step 3: read_particle_info()
   * step 4: read_domain_info()
   * step 5: read_force_info()
   * step 6: read_ggem_info()
   * step 7: read_solver_info()
   * step 8: read_chebyshev_info()
   * step 9: read_run_info()
   */
  void            read_input();


  /*!
   * Create object_mesh
   * this object_mesh can be point_mesh or particle_mesh
   * step 1: create_domain_mesh()
   * step 2: create_periodid_boundary()
   * step 3: create_object()
   * step 4: create object_mesh by combining domain mesh, object and periodic
   *boundary
   */
  virtual void    create_object_mesh() = 0;

  /*!
   * Create equation_system object
   * step 1: initialize equation_systems using mesh;
             add a PMLinearImplicitSystem, system, to equation_systems;
             add variables
   * step 3: attach_object_mesh()
   * step 4: attach_period_boundary()
   * step 5: initialize equation systems and preconditioning matrix
   * step 5: set_parameters() for equation_systems
   * step 6: Initialize the force field
   */
  EquationSystems create_equation_systems();

  /*!
   * integrater
   * step 1:
   */
  virtual void    run(EquationSystems& equation_systems) = 0;


  /*!
   * destroy PETSC objects
   */
  void destroy();

protected:

  /*!
   * Steps for read_input()
   */

  void         read_system_info();
  void         read_module_info();
  void         read_physical_info();
  virtual void read_particle_info() = 0;
  void         read_domain_info();
  void         read_force_info();
  virtual void read_ggem_info() = 0;
  void         read_solver_info();
  void         read_chebyshev_info();
  void         read_run_info();
  void         read_restart_time();
  void         read_restart_eigenvalue();

  /*!
   * Steps for create_object_mesh()
   */
  void         create_domain_mesh();
  void         create_periodic_boundary();
  virtual void create_object() = 0;

  /*!
   * Steps for create_equation_systems
   */
  virtual void attach_object_mesh(PMLinearImplicitSystem& system) = 0;
  void         attach_period_boundary(PMLinearImplicitSystem& system,
    std::vector<std::string>& periodic_vars);
  virtual void set_parameters(EquationSystems& equation_systems) = 0;

  // force field
  void         attach_fixes(PMLinearImplicitSystem& system);

  /*!
   * Steps for integrate()
   */

  /*!
   * Compute undisturbed velocity field without particles.
   * NOTE: We MUST re-init particle-mesh before solving Stokes
   */
  void solve_undisturbed_system(EquationSystems& equation_systems);

  /*!
   * Create vectors and Shell Mat for use
   * Create brownian system
   */
  void create_brownian_system(EquationSystems& equation_systems);


  /*!
   * Get minimum dt from all systems
   */
  const Real get_min_dt(EquationSystems& es,
    std::vector<std::string> sys_names = std::vector<std::string>());

  /*!
   * Integrate particle motions using Fixman's midpoint scheme
   */
  void fixman_integrate(EquationSystems& equation_systems,
                        unsigned int   & i);

  /*!
   * Integrate particle motions using overdamped Langevin(Brownian dynamics)
   *scheme
   */
  void langevin_integrate(EquationSystems& equation_systems,
                          unsigned int   & i);

  // update object positions due to PBS
  virtual void update_object() {}

  virtual void write_object(unsigned int step_id) = 0;

  // output precision
  int o_precision;
  
  // copss generated slit mesh boundary id
  // x-negative surface boundary_id = 4
  // x-positive surface boundary_id = 2
  // y-negative surface boundary_id = 1
  // y-positive surface boundary_id = 3
  // z-negative surface boundary_id = 0
  // z-positive surface boundary_id = 5
  std::vector<boundary_id_type> copss_slitMesh_boundary_id{4, 2, 1, 3, 0, 5};
  
  // Unify slit mesh boundary id to the following order
  // x-negative surface boundary_id = 4
  // x-positive surface boundary_id = 6
  // y-negative surface boundary_id = 3
  // y-positive surface boundary_id = 5
  // z-negative surface boundary_id = 2
  // z-positive surface boundary_id = 1
  std::vector<boundary_id_type> slitMesh_boundary_id{4, 6, 3, 5, 2, 1};
};
} // end namespace libMesh
