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


#include <iomanip> // setw | setprecision

#include "libmesh/elem.h"

#include "point_particle.h"
#include "pm_toolbox.h"


namespace libMesh {
// ======================================================================
PointParticle::PointParticle(const Point       pt,
                             const dof_id_type point_id)
  : _center(pt), _counter(3, 0), _id(point_id),
  _point_type(NOT_DEFINED_POINTTYPE), _parent_id(-1),
  _processor_id(-1),
  _elem_id(-1),
  _force(0.),
  _velocity(0.),
  _orientation(0.),
  _constant_force(0.),
  _charge(0.)
{
  // do nothing
}

// ======================================================================
PointParticle::PointParticle(const Point       pt,
                             const dof_id_type point_id,
                             const PointType   point_type)
  : _center(pt), _counter(3, 0), _id(point_id),
  _parent_id(-1), _point_type(point_type),
  _processor_id(-1),
  _elem_id(-1),
  _force(0.),
  _velocity(0.),
  _orientation(0.),
  _constant_force(0.),
  _charge(0.)
{
  // do nothing
}

// ======================================================================
PointParticle::PointParticle(const Point              pt,
                             const dof_id_type        point_id,
                             const PointType          point_type,
                             const std::vector<Real>& rot_vec)
  : _center(pt), _counter(3, 0), _id(point_id),
  _parent_id(-1), _point_type(point_type),
  _processor_id(-1),
  _elem_id(-1),
  _force(0.),
  _velocity(0.),
  _orientation(rot_vec),
  _constant_force(0.),
  _charge(0.)
{
  // do nothing
}

// ======================================================================
PointParticle::PointParticle(const Point              pt,
                             const dof_id_type        point_id,
                             const PointType          point_type,
                             const std::vector<Real>& rot_vec,
                             const Point              constant_force,
                             const Real               charge)
  : _center(pt), _counter(3, 0), _id(point_id),
  _parent_id(-1), _point_type(point_type),
  _processor_id(-1),
  _elem_id(-1),
  _force(0.),
  _velocity(0.),
  _orientation(rot_vec),
  _constant_force(constant_force),
  _charge(charge)
{
  // do nothing
}

// ======================================================================
PointParticle::PointParticle(const PointParticle& particle)
{
  _center         = particle._center;
  _counter        = particle._counter;
  _id             = particle._id;
  _point_type     = particle._point_type;
  _parent_id      = particle._parent_id;
  _processor_id   = particle._processor_id;
  _elem_id        = particle._elem_id;
  _neighbor_list  = particle._neighbor_list;
  _force          = particle._force;
  _velocity       = particle._velocity;
  _orientation    = particle._orientation;
  _constant_force = particle._constant_force;
  _charge         = particle._charge;
}

// ======================================================================
PointParticle::~PointParticle()
{
  // do nothing
}

// ======================================================================
void PointParticle::set_particle_force(const Point& pforce)
{
  _force = pforce;
}

// ======================================================================
void PointParticle::add_particle_force(const Point& pforce)
{
  _force += pforce;
}

// ======================================================================
void PointParticle::zero_particle_force()
{
  _force.zero();
}

// ======================================================================
void PointParticle::reinit_particle()
{
  START_LOG("PointParticle::reinit_particle()", "PointParticle");

  // reset int
  _processor_id = -1;
  _elem_id      = -1;
  _neighbor_list.clear();
  STOP_LOG("PointParticle::reinit_particle()", "PointParticle");
}

// ======================================================================
void PointParticle::set_orientation(const std::vector<Real>& rot_vec)
{
  _orientation = rot_vec;
}

// ======================================================================
void PointParticle::print_info(const Parallel::Communicator& comm_in) const
{
  std::ostringstream oss;
  oss << "--------> point particle[" << _id << "]\n";
  oss << "     center = (" << _center(0) << "," << _center(1) << "," <<
  _center(2) <<")\n";
  oss << "     PBC Counter = (" << _counter[0] << "," << _counter[1] << "," <<
      _counter[2] <<")\n";
  oss << "     force = (" << _force(0) << "," << _force(1) << "," <<
      _force(2) <<")\n";
  oss << "     velocity = (" << _velocity(0) << "," << _velocity(1) << "," <<
      _velocity(2) <<")\n";
  oss << "     orientation = (" << _orientation[0] << "," << _orientation[1] <<
  "," << _orientation[2] <<"\n";
  oss << "     constant force = (" << _constant_force(0) << "," << _constant_force(1) <<
  "," << _constant_force(2) <<")\n";
  oss << "     charge = " << _charge;
  oss << "     parent_id = " << _parent_id;
  oss << "     point_type = " << _point_type;
  oss << "     elem_id = " << _elem_id;

  // output the neighbor list
  if (_neighbor_list.size() > 0)
  {
    oss << "      its neighbor points include: ";
    for (dof_id_type i = 0; i < _neighbor_list.size(); ++i)
      oss << _neighbor_list[i] <<", ";
    oss <<"\n";
  }
  else oss << "      there are no neighbors around this point particle!\n";

  PMToolBox::output_message(oss, comm_in);
} // the end of print_info()
}   // end of namespace
