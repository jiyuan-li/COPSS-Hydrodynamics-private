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

#include "copss.h"

using std::cout;
using std::endl;
using std::string;

namespace libMesh{

class GeneralPointParticleSystem : public Copss
{
public:
	
	GeneralPointParticleSystem (int argc, char** argv);

	void build_system();

protected:
	std::string particle_type; // particle type (either finite_size_particle or point_particle)
	std::string point_particle_model;
	// ===========for beads
	unsigned int Nb; // total # of beads
	unsigned int Ns; // total # of springs per Chain
	// ===========for polymer chains
	unsigned int nChains; // total # of chains
	Real bk; // Kuhn length (um)
	Real Nks; // # of Kuhn length per Chain
	Real Ss2; // (um^2)
	Real q0; //maximum spring length (um)
	Real chain_length; // contour length of the spring (um)
	Real Dc; // Diffusivity of the chain (um^2/s)

	//void read_data(std::string control_file){};
	// override read_particle_parameters() function in Copss class
	void read_particle_info () override;


};


}
