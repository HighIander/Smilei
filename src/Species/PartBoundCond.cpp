#include "PartBoundCond.h"

#include <cstdlib>

#include <iostream>
#include <string>

#include <cmath>

#include "Particles.h"
#include "BoundaryConditionType.h"
#include "Patch.h"
#include "Tools.h"

using namespace std;

PartBoundCond::PartBoundCond( Params& params, Species *species, Patch* patch ) :
    isRZ( params.geometry == "3drz" )
{   
    // number of dimensions for the particle
    //!\todo (MG to JD) isn't it always 3?
    nDim_particle = params.nDim_particle;
    nDim_field = params.nDim_field;
    // Absolute global values
    double x_min_global = 0;
    double x_max_global = params.cell_length[0]*(params.n_space_global[0]);
    double y_min_global = 0;
    double y_max_global = params.cell_length[1]*(params.n_space_global[1]);
    double z_min_global = 0;
    double z_max_global = params.cell_length[2]*(params.n_space_global[2]);

    // by default apply no bcs
    bc_xmin  = NULL;
    bc_xmax  = NULL;
    bc_ymin  = NULL;
    bc_ymax  = NULL;
    bc_zmin  = NULL;
    bc_zmax  = NULL;

    // -----------------------------
    // Define limits of local domain
    if (params.EM_BCs[0][0]=="periodic" || params.hasWindow) {
        x_min = patch->getDomainLocalMin(0);
        x_max = patch->getDomainLocalMax(0);
        MESSAGE("xmin max");
    }
    else {
        x_min = max( x_min_global, patch->getDomainLocalMin(0) );
        x_max = min( x_max_global, patch->getDomainLocalMax(0) );

	}

    if ( nDim_particle > 1 ) {
        if (params.EM_BCs[1][0]=="periodic") {
            y_min = patch->getDomainLocalMin(1);
            y_max = patch->getDomainLocalMax(1);
        }
        else {
            y_min = max( y_min_global, patch->getDomainLocalMin(1) );
            y_max = min( y_max_global, patch->getDomainLocalMax(1) );
        MESSAGE("YMIN MAX");
	}
        
        if ( ( nDim_particle > 2 ) && (!isRZ) ) {
            if (params.EM_BCs[2][0]=="periodic") {
                z_min = patch->getDomainLocalMin(2);
                z_max = patch->getDomainLocalMax(2);
            }
            else {
                z_min = max( z_min_global, patch->getDomainLocalMin(2) );
                z_max = min( z_max_global, patch->getDomainLocalMax(2) );
            }
        }
    }

    MESSAGE("hey there !");
    // Can be done after parsing
    MESSAGE(! species->particles->tracked);
    // Check for inconsistencies between EM and particle BCs
    if (! species->particles->tracked) {
        for( unsigned int iDim=0; iDim<(unsigned int)nDim_field; iDim++ ) {
            if ( ((params.EM_BCs[iDim][0]=="periodic")&&(species->boundary_conditions[iDim][0]!="periodic"))  
             ||  ((params.EM_BCs[iDim][1]=="periodic")&&(species->boundary_conditions[iDim][1]!="periodic")) ) {
                ERROR("For species " << species->name << ", periodic EM "<<"xyz"[iDim]<<"-boundary conditions require particle BCs to be periodic.");
        MESSAGE("check");    
        }
        }
    }
    MESSAGE("CHECK");
    // ----------------------------------------------
    // Define the kind of applied boundary conditions
    // ----------------------------------------------
    
    int (*remove)  ( Particles &, int , int , double , Species *, double &);
    if( species->mass == 0 ) {
        remove = &remove_photon;
    } else {
        remove = &remove_particle;
    }
    MESSAGE("check remove ");
    // Xmin
    if ( species->boundary_conditions[0][0] == "reflective" ) {
        if (patch->isXmin()) bc_xmin = &reflect_particle;
    }
    else if ( species->boundary_conditions[0][0] == "remove" ) {
        if (patch->isXmin()) bc_xmin = remove;
        MESSAGE("Xmin and max");    
    }
    else if ( species->boundary_conditions[0][0] == "stop" ) {
        if (patch->isXmin()) bc_xmin = &stop_particle;
    }
    else if ( species->boundary_conditions[0][0] == "thermalize" ) {
        if (patch->isXmin()) bc_xmin = &thermalize_particle;
    }
    else if ( species->boundary_conditions[0][0] == "periodic" ) {
    }
    else {
        ERROR("Xmin boundary condition `"<<species->boundary_conditions[0][0]<<"` unknown" );
    }
    
    // Xmax
    if ( species->boundary_conditions[0][1] == "reflective" ) {
        if (patch->isXmax()) bc_xmax = &reflect_particle;
    }
    else if ( species->boundary_conditions[0][1] == "remove" ) {
        if (patch->isXmax()) bc_xmax = remove;
        MESSAGE("Xmin and max");    
    }
    else if ( species->boundary_conditions[0][1] == "stop" ) {
        if (patch->isXmax()) bc_xmax = &stop_particle;
    }
    else if ( species->boundary_conditions[0][1] == "thermalize" ) {
        if (patch->isXmax()) bc_xmax = &thermalize_particle;
    }
    else if ( species->boundary_conditions[0][1] == "periodic" ) {
    }
    else {
        ERROR( "Xmax boundary condition `"<<species->boundary_conditions[0][1]<<"`  unknown" );
    }

    
    if ( ( nDim_particle > 1 ) && (!isRZ) ) {
        // Ymin
        if ( species->boundary_conditions[1][0] == "reflective" ) {
            if (patch->isYmin()) bc_ymin = &reflect_particle;
        }
        else if ( species->boundary_conditions[1][0] == "remove" ) {
            if (patch->isYmin()) bc_ymin = remove;
        }
        else if ( species->boundary_conditions[1][0] == "stop" ) {
            if (patch->isYmin()) bc_ymin = &stop_particle;
        }
        else if ( species->boundary_conditions[1][0] == "thermalize" ) {
            if (patch->isYmin()) bc_ymin = &thermalize_particle;
        }
        else if ( species->boundary_conditions[1][0] == "periodic" ) {
        }
        else {
            ERROR( "Ymin boundary condition `"<< species->boundary_conditions[1][0] << "` unknown"  );
        }

        // Ymax
        if ( species->boundary_conditions[1][1] == "reflective" ) {
            if (patch->isYmax()) bc_ymax = &reflect_particle;
        }
        else if ( species->boundary_conditions[1][1] == "remove" ) {
            if (patch->isYmax()) bc_ymax = remove;
        }
        else if ( species->boundary_conditions[1][1] == "stop" ) {
            if (patch->isYmax()) bc_ymax = &stop_particle;
        }
        else if ( species->boundary_conditions[1][1] == "thermalize" ) {
            if (patch->isYmax()) bc_ymax = &thermalize_particle;
        }
        else if ( species->boundary_conditions[1][1] == "periodic" ) {
        }
        else {
            ERROR( "Ymax boundary condition `"<< species->boundary_conditions[1][1] <<"` undefined" );
        }
        
        
        if ( nDim_particle > 2 ) {
            if ( species->boundary_conditions[2][0] == "reflective" ) {
                if (patch->isZmin()) bc_zmin = &reflect_particle;
            }
            else if ( species->boundary_conditions[2][0] == "remove" ) {
                if (patch->isZmin()) bc_zmin = remove;
            }
            else if ( species->boundary_conditions[2][0] == "stop" ) {
                if (patch->isZmin()) bc_zmin = &stop_particle;
            }
            else if ( species->boundary_conditions[2][0] == "periodic" ) {
            }
            else {
                ERROR( "Zmin boundary condition `"<< species->boundary_conditions[2][0] << "` unknown"  );
            }
            
            if ( species->boundary_conditions[2][1] == "reflective" ) {
                if (patch->isZmax()) bc_zmax = &reflect_particle;
            }
            else if ( species->boundary_conditions[2][1] == "remove" )  {
                if (patch->isZmax()) bc_zmax = remove;
            }
            else if ( species->boundary_conditions[2][1] == "stop" ) {
                if (patch->isZmax()) bc_zmax = &stop_particle;
            }
            else if ( species->boundary_conditions[2][1] == "periodic" ) {
            }
            else {
                ERROR( "Zmax boundary condition `"<< species->boundary_conditions[2][1] << "` unknown"  );
            }
            
        }//nDim_particle>2
        
    }//nDim_particle>1
    else if (isRZ) {
        #ifdef _TODO_RZ
        // rmin !!!
        //        §§ none !!!
        #endif
        // Ymin
        //if ( species->boundary_conditions[1][0] == "reflective" ) {
        //    if (patch->isYmin()) bc_ymin = &refl_particle_rz;
        //}
        //else if ( species->boundary_conditions[1][0] == "remove" ) {
        //    if (patch->isYmin()) bc_ymin = &remove_particle;
        //}
        //else if ( species->boundary_conditions[1][0] == "stop" ) {
        //    if (patch->isYmin()) bc_ymin = &stop_particle_rz;
        //}
        //else if ( species->boundary_conditions[1][0] == "none" ) {
        //    if (patch->isMaster()) MESSAGE(2,"Ymin boundary condition for species " << species->name << " is 'none', which means the same as fields");
        //}
        //else {
        //    ERROR( "Ymin boundary condition undefined : " << species->boundary_conditions[1][0]  );
        //}

       MESSAGE(isRZ); 
        // Ymax
       // if ( species->boundary_conditions[1][1] == "reflective" ) {
       //     if (patch->isYmax()) bc_ymax = &refl_particle_rz;
       // }
         if ( species->boundary_conditions[1][1] == "remove" ) {
            if (patch->isYmax()) bc_ymax = &remove_particle;
		MESSAGE("Rmax bc");
        }
        //else if ( species->boundary_conditions[1][1] == "stop" ) {
        //    if (patch->isYmax()) bc_ymax = &stop_particle_rz;
        //}
        //else if ( species->boundary_conditions[1][1] == "none" ) {
        //    if (patch->isMaster()) MESSAGE(2,"Ymax boundary condition for species " << species->name << " is 'none', which means the same as fields");
        //}
        else {
            //ERROR( "Ymax boundary condition undefined : " << species->boundary_conditions[1][1]  );
            ERROR("Only Remove boundary conditions can be applied to particles in Rz geometry ");
	}
    }
    
    
}


PartBoundCond::~PartBoundCond()
{
}

