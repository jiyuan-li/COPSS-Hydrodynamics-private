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

#include <stdio.h>
#include <map>
#include <cmath>

#include "fix.h"
#include "../point_particle.h"

namespace libMesh {
/*
 * The class is designed for computing the force field
 * of the system
 */


class FixPoint : public Fix {
public:

  // ! Constructor for a system with point particles
  FixPoint(PMLinearImplicitSystem& pm_sys);

  virtual ~FixPoint() {}

  /*! Prepare for running
     /*
   * Check if particle_type == "point_particle"
   * this -> check_params()
   */
  void         initParticleType();

  virtual void initPointParticleType() {}

  void         check_walls();

  void         check_walls_pbcCount();

protected:

  std::string point_particle_model;
}; // end of class
} // end of namespace
