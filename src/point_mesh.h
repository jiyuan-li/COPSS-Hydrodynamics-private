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
#include "libmesh/libmesh_common.h"
#include "libmesh/reference_counted_object.h"
#include "libmesh/parallel_object.h"
#include "libmesh/mesh.h"
#ifdef LIBMESH_HAVE_NANOFLANN
# include "libmesh/nanoflann.hpp"
#endif // ifdef LIBMESH_HAVE_NANOFLANN


// Local Includes   -----------------------------------
#include "point_particle.h"
#include "polymer_chain.h"
#include "pm_periodic_boundary.h"
#include "rigid_particle.h"

// C++ Includes   -------------------------------------
// #include <stdio.h>
#include <string>
#include <vector>


namespace libMesh {
class PointParticle;

template<unsigned int KDDim>
class ParticleMesh;

/*
 * This template class defines point-point and point-mesh mapping relations.
 * It takes advantage of KD Tree library nanoflann to build the neigbor list.
 */

template<unsigned int KDDim>
class PointMesh :  public ReferenceCountedObject<PointMesh<KDDim> >,
                   public ParallelObject {
protected:

#ifdef LIBMESH_HAVE_NANOFLANN

  /**
   * This 'embeded in' class adapts list of libMesh \p Particle types
   * for use in a nanoflann KD-Tree. For more on the basic idea, see
   * examples/pointcloud_adaptor_example.cpp in the nanoflann src.
   * This class is similar to the "PointCloud" in the example.
   */

  template<unsigned int PLDim>
  class PointListAdaptor {
private:

    // use ref, which will point to the address of _particles in the parent
    // class
    // when _particles is modified, this ref needs not to be updated
    const std::vector<PointParticle *>& _pts;

public:

    // Constructor of PointListAdaptor
    PointListAdaptor(const std::vector<PointParticle *>& particles)
      : _pts(particles)
    {}

    /**
     * libMesh \p Point coordinate type and std::size_t type
     */
    typedef Real        coord_t;
    typedef std::size_t size_t;

    /**
     * Must return the number of data points
     */
    inline size_t kdtree_get_point_count() const {
      return _pts.size();
    }

    /**
     * Returns the distance between the vector "p1[0:size-1]"
     * and the data point with index "idx_p2" stored in the class
     */
    inline coord_t kdtree_distance(const coord_t *p1,
                                   const size_t   idx_p2,
                                   size_t         size) const
    {
      libmesh_assert_equal_to(size, PLDim);
      libmesh_assert_less(idx_p2, _pts.size());

      // retrieve the point data with index = idx_p2
      const Point& p2(_pts[idx_p2]->point());

      switch (size)
      {
      case 3:
      {
        const coord_t d0 = p1[0] - p2(0);
        const coord_t d1 = p1[1] - p2(1);
        const coord_t d2 = p1[2] - p2(2);
        return d0 * d0 + d1 * d1 + d2 * d2;
      }

      case 2:
      {
        const coord_t d0 = p1[0] - p2(0);
        const coord_t d1 = p1[1] - p2(1);
        return d0 * d0 + d1 * d1;
      }

      case 1:
      {
        const coord_t d0 = p1[0] - p2(0);
        return d0 * d0;
      }

      default:
        libmesh_error_msg("ERROR: unknown size " << size);
      }

      return -1.0;
    } // end of kdtree_distance()

    /**
     * Returns the dim'th component of the idx'th point in the class:
     * Since this is inlined and the "dim" argument is typically an immediate
     *value, the
     *  "if's" are actually solved at compile time.
     */
    inline coord_t kdtree_get_pt(const size_t idx, int dim) const
    {
      libmesh_assert_less(dim, (int)PLDim);
      libmesh_assert_less(idx, _pts.size());
      libmesh_assert_less(dim, 3);

      const Point& p(_pts[idx]->point());

      if (dim == 0) return p(0);

      if (dim == 1) return p(1);

      return p(2);
    } // end of kdtree_get_pt()

    /**
     * Optional bounding-box computation: return false to default to a standard
     *bbox computation loop.
     * Return true if the BBOX was already computed by the class and returned in
     *"bb" so it can be
     * avoided to redo it again. Look at bb.size() to find out the expected
     *dimensionality
     * (e.g. 2 or 3 for point clouds)
     */
    template<class BBOX>
    bool kdtree_get_bbox(BBOX& /* bb */) const {
      return false;
    }
  }; // end of class PointListAdaptor


  typedef nanoflann::KDTreeSingleIndexAdaptor<nanoflann::L2_Simple_Adaptor<Real,
                                                                           PointListAdaptor<KDDim> >,
                                              PointListAdaptor<KDDim>,
                                              KDDim>kd_tree_t;

  /*
   *  If a data member is declared mutable, then it is legal to assign a value
     to this data member from a const member function.
   */
  mutable UniquePtr<kd_tree_t>_kd_tree;

#endif // LIBMESH_HAVE_NANOFLANN


  /**
   * Build & initialize the KD tree through nanoFlann, if needed.
   */
  virtual void construct_kd_tree();

public:

  /* Constructor
   * This creates the PointMesh class using the ParticleMesh object,
   * but does NOT change the ParticleMesh
   */
  PointMesh(libMesh::ParticleMesh<KDDim>& particle_mesh,
            const Real                  & search_radius_p,
            const Real                  & search_radius_e);


  /*
   * Constructor
   * This creates the PointMesh class using multiple PolymerChains
   * FIXME: NOT used currently, and need to be modified!
   */
  PointMesh(MeshBase                 & mesh,
            std::vector<PolymerChain>& polymer_chains,
            const Real               & search_radius_p,
            const Real               & search_radius_e);


  /*
   * Constructor
   * This creates the PointMesh class using single PolymerChain
   */
  PointMesh(MeshBase    & mesh,
            PolymerChain& polymer_chain,
            const Real  & search_radius_p,
            const Real  & search_radius_e);


  /*
   * Destructor
   */
  ~PointMesh();


  /**
   * Return the information of particles
   */
  std::vector<PointParticle *>particles() const {
    return _particles;
  }

  /**
   * Return the information of polymer chain
   */
  PolymerChain* polymer_chain() const {
    return _polymer_chain;
  }

  /**
   * Return the total number of particles
   */
  std::size_t num_particles() const {
    return _particles.size();
  }

  std::size_t num_chains()  const {
    return _polymer_chain->n_chains();
  }

  /**
   * Return the total number of bonds
   */
  std::size_t num_bonds() const {
    return _polymer_chain->n_bonds();
  }

  /**
   * Check if the neighbor list is constructed sortedly
   */
  bool is_sorted() const {
    return _is_sorted;
  }

  /**
 * Clear the KD-Tree if need
 * Note, different from the construct_kd_tree, this is a public function.
 */
  virtual void clear_kd_tree();


  /**
   * Return the information of element neighbor list
   * around the search radius _search_radius_e
   * Mapping between elem id and element neighbor list of particles
   */
//  const std::map<const dof_id_type,
//                 std::vector<dof_id_type> >& elem_neighbor_list() const
//  {
//    return _elem_point_neighbor_list;
//  }

  /**
   * Return the particle neighbor list around a given element
   * around the search radius _search_radius_e
   */
//  const std::vector<std::size_t>elem_neighbor_list(const Elem *elem)
//  {
//    const std::size_t elem_id = elem->id();
//
//    return _elem_neighbor_list[elem_id];
//
//    // return _elem_neighbor_list.at(elem_id); // not work well
//  }

  /**
   * Return the particle neighbor list around a given LOCAL element
   * around the search radius _search_radius_e
   */
//  const std::vector<std::size_t>local_elem_neighbor_list(const Elem *elem)
//  {
//    const std::size_t elem_id = elem->id();
//
//    return _local_elem_neighbor_list[elem_id];
//  }


  /**
   * Build the neighbor list of a particle within the search_radius_p using
   *KD-tree.
   * This includes the query particle itself, if the query_pt is also in the
   *particle list.
   * For example, the coords of the query_pt is the same as that of a particle
   *in the list.
   *
   * IndicesDists output pair<particle_id, distance>
   * Note we need the particle distance to compute their interaction forces.
   */
  void build_particle_neighbor_list(
    const Point                  & query_pt,
    const bool                     is_sorted,
    std::vector<std::pair<std::size_t,Real> >& IndicesDists);


  /**
   * Build the particle-particle neighbor list for all the particles with the
   * search_radius_p using KD tree(logN complexity).
   *
   * This directly call the function above!
   * In fact, the particle neighbor list has been built in "reinit()", so this
   *is NOT USED!
   */
//  void build_particle_neighbor_list();


  /*** NOT ACTUALLY USED NOW, written only for the purpose of comparing with
     KD-Tree!
   *
   * Build the neighbor list for all the particles directly! (N^2 complexity)
   * NOTE that the particle list constructed from this function is not sorted!
   ***/
//  void build_particle_neighbor_list_naively();

  /**
   * reinit neighbor distance for all particles
   */
  void reinit_neighbor_vector();

  /**
   * Build the neighbor list of partilces for a given elem
   * Output the n_list,
   * which gives the particle ids around elem's centroid within search_radius_e.
   *
   * Note we don't need the distance between particles and elem centroid here,
   * because this list is only used to evaluate element-wise load vector due to
   * the neighboring particles.
   */
//  void build_elem_neighbor_list(const Elem               *elem,
//                                        const bool                is_sorted,
//                                        std::vector<std::size_t>& n_list);


  /** FIXME: Should we build this locally and globally(by allgather)???
   *
   * Build the particle neighbor list for each element within the search radius
   * this will generate the _elem_neighbor_list and  _elem_neighbor_list_vector
   * NOTE this takes advantage of KD Tree
   */
//  void build_elem_neighbor_list();


  /**
   * Reinit the point-mesh system.
   */
  void reinit(bool& neighbor_list_update_flag);


  /*
   * update the (input) particle_mesh object according to the point_mesh
   * This function updates the input class, but doesn't change its own data
   */
  void update_particle_mesh(ParticleMesh<KDDim> *particle_mesh) const;


  /*
   * update the point_mesh object according to the (input) particle_mesh.
   * This function updates the class itself only, but doesn't change the input
   */

  // void update_point_mesh(const ParticleMesh<KDDim>* particle_mesh);


  /**
   * print out the particle information
   */
  void print_point_info() const;


  /**
   * print out the element neighbor list information
   */
  void print_elem_neighbor_list(std::ostream& out = libMesh::out) const;


  /**
   * Set the values of search radii
   */
  void set_search_radius(const Real rp, const Real re)
  {
    _search_radius_p = rp;
    _search_radius_e = re;
  }

  /**
   * Return the search radius
   */
  Real search_radius(const std::string& p_e) const;


  /**
   * Add periodic boundary condition
   */
  void add_periodic_boundary(PMPeriodicBoundary& _periodic_bdry)
  {
    _periodic_boundary = &_periodic_bdry;
  }

  /**
   * Retrun the pointer to the periodic boundary for use
   */
  PMPeriodicBoundary* pm_periodic_boundary()
  {
    return _periodic_boundary;
  }

  /**
   * Set particle (bead or surface node) velocity
   * vel.size() = nui
   */
  void set_bead_velocity(const std::vector<Real>& vel);

  /**
   * Set rigid particle velocity
   * vel.size() = nui
   */

  // void set_rigid_particle_velocity();


  /**
   * Calculate maximum point particle (or rigid particle surface nodes) velocity
   */
  const Real& maximum_bead_velocity() const {
    return _max_velocity_magnitude;
  }

  /**
   * Calculate minimum point particle (or rigid particle surface nodes) velocity
   */
  const Real& minimum_bead_velocity() const {
    return _min_velocity_magnitude;
  }

  /**
   * Write bead positions to csv file
   */
  const void write_initial_surface_node_pos() const;


  /**
   * Get the ids of all neighbor element of an element
   */
  const std::vector<dof_id_type> get_elem_elem_neighbor_list(const
                                                             dof_id_type& elem_id)
  {
    return _elem_elem_neighbor_list[elem_id];
  };

  /**
   * Build element element neighbor list
   */
  void build_elem_elem_neighbor_list();

  /**
   * Set element element neighbor list
   */
  void set_elem_elem_neighbor_list(const std::map<const dof_id_type,
    std::vector<dof_id_type>>& elem_elem_neighbor_list)
  {
    _elem_elem_neighbor_list = elem_elem_neighbor_list;
  };


  /**
   * Build node to element mapping
   */
   void build_node_elem_mapping();

   /**
    * Get element global id of a node
    * notice that the input node id is global
    */
   const dof_id_type get_node_elem_id(const dof_id_type& node_id)
   {
     return _node_elem_mapping[node_id];
   }


  /**
   * Get the ids of all points within an element
   */
  const std::vector<dof_id_type> get_elem_point_containing_list(const
                                                                dof_id_type& elem_id)
  {
    if (_elem_point_containing_list.count(elem_id))
      return _elem_point_containing_list[elem_id];
    else
      return std::vector<dof_id_type>();
  }


  /**
   * Get the ids of all neighbor points of an element
   */
  const std::vector<dof_id_type> get_elem_point_neighbor_list(const
                                                              dof_id_type& elem_id)
  {
    return _elem_point_neighbor_list[elem_id];
  }

  /**
   * Build element particle neighbor list.
   */
  void build_elem_point_neighbor_list();

  /**
   * Build particle-particle neighbor list
   * This is done by first locate the element that this point is within and
   * then find all neighbor points of this element
   */
   void build_point_point_neighbor_list();

   const MeshBase& get_mesh()
   {
     return _mesh;
   }

protected:

  /**
   * Build element particle containing list. This function will be called
   * within build_elem_point_neighbor_list().
   */
  void build_elem_point_containing_list();

private:

  // Mesh base (for simulaiton domain)
  MeshBase& _mesh;

  // Search radius (around a particle)
  Real _search_radius_p;

  // Search radius (around an element) whose value must be larger than hmax!
  Real _search_radius_e;

  // A vector that store the pointers to PointParticle(save storage)
  std::vector<PointParticle *>_particles;

  // A vector that store the pointers to RigidParticle(save storage)
  std::vector<RigidParticle *>_rigid_particles;

  // number of point particles
  std::size_t _num_point_particles;

  // number of rigid particles
  std::size_t _num_rigid_particles;

  // Polymer chain
  PolymerChain *_polymer_chain = nullptr;

  // The point list adapter
  // - interface to the nanoflann in order to construct KD-Tree
  PointListAdaptor<KDDim>_point_list_adaptor;

  // Is the neighbor list sorted? set true defaultly (particle/elem neighbor
  // list)
  bool _is_sorted;

  // Element neighbor list: mapping between Elem and particle id's around it
  // This is NOT necessary to be on all the processors in implementation,
  // but we do this here only for test purpose. For local build
//  std::map<const std::size_t, std::vector<std::size_t> >_elem_neighbor_list;
//  std::map<const std::size_t, std::vector<std::size_t> >_local_elem_neighbor_list;

  // elem-elem neighbor list: mapping between the id of one element to the
  // ids of all its neighbor element. Notice that this mapping does change
  // with time. This mapping is global, i.e., all processors have the same
  // information about _elem_elem_neighbor_list
  std::map<const dof_id_type, std::vector<dof_id_type>>_elem_elem_neighbor_list;

  // mapping between node id and the element id it belongs to. Notice that we
  // only take one of the element if a node is shared by multiple elements.
  // This mapping will only be created once; Notice that both node_id and
  // element id in the mapping are global; All processors have the same
  // information about this mapping
  std::map<const dof_id_type, dof_id_type> _node_elem_mapping;

  // mapping between the id of one element to the ids of all the point
  // particles within this element. Notice that this mapping changes with
  // time. This mapping is global, i.e., all processors have the same
  // information about _elem_point_containing_list
  std::map<const dof_id_type, std::vector<dof_id_type>>_elem_point_containing_list;

  // mapping between the id of one element to the ids of its neighbor
  // particles. We find this mapping by first finding the neighbor elements
  // of this element and then look for particles located with each of the
  // neighbor elements and itself. This mapping is global, i.e., all processors
  // have the same information about _elem_point_neighbor_list. Notice that
  // the neighbor points include points within the element.
  std::map<const dof_id_type, std::vector<dof_id_type>>_elem_point_neighbor_list;

  // mapping between the id of particle to its neighbor points. We find this
  // mapping by first finding the element that contains this point and then
  // look for the neighbor points of this element from
  // _elem_point_neighbor_list. This mapping is global and excludes particle
  // itself from the neighbor list.
  std::map<const dof_id_type,std::vector<dof_id_type>>_point_point_neighbor_list;

  // Pointer to the Periodic boundary condition
  PMPeriodicBoundary *_periodic_boundary = nullptr;

  // bead velocity magniture array
  std::vector<Real>_velocity_magnitude;

  // maximum bead/surface node velocity magniture
  Real _max_velocity_magnitude;

  // minimum bead/surface node velocity magniture
  Real _min_velocity_magnitude;
}; // end of the class
} // end name space
