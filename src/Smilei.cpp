////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////                                                                                                                ////
////                                                                                                                ////
////                                   PARTICLE-IN-CELL CODE SMILEI                                                 ////
////                    Simulation of Matter Irradiated by Laser at Extreme Intensity                               ////
////                                                                                                                ////
////                          Cooperative OpenSource Object-Oriented Project                                        ////
////                                      from the Plateau de Saclay                                                ////
////                                          started January 2013                                                  ////
////                                                                                                                ////
////                                                                                                                ////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <ctime>
#include <cstdlib>
#include <unistd.h>
#include <iostream>
#include <iomanip>
#include <string>
#include <omp.h>

#include "Smilei.h"
#include "SmileiMPI_test.h"
#include "Params.h"
#include "PatchesFactory.h"
#include "SyncVectorPatch.h"
#include "Checkpoint.h"
#include "Solver.h"
#include "SimWindow.h"
#include "Diagnostic.h"
#include "Domain.h"
#include "SyncCartesianPatch.h"
#include "Timers.h"
#include "RadiationTables.h"
#include "MultiphotonBreitWheelerTables.h"

using namespace std;

// ---------------------------------------------------------------------------------------------------------------------
//                                                   MAIN CODE
// ---------------------------------------------------------------------------------------------------------------------
int main (int argc, char* argv[])
{
    cout.setf( ios::fixed,  ios::floatfield ); // floatfield set to fixed

    // -------------------------
    // Simulation Initialization
    // -------------------------

    // Create MPI environment :

#ifdef SMILEI_TESTMODE
    SmileiMPI_test smpi( &argc, &argv );
#else
    SmileiMPI smpi(&argc, &argv );
#endif

    MESSAGE("                   _            _");
    MESSAGE(" ___           _  | |        _  \\ \\   Version : " << __VERSION);
    MESSAGE("/ __|  _ __   (_) | |  ___  (_)  | |   ");
    MESSAGE("\\__ \\ | '  \\   _  | | / -_)  _   | |");
    MESSAGE("|___/ |_|_|_| |_| |_| \\___| |_|  | |  ");
    MESSAGE("                                /_/    ");
    MESSAGE("");

    // Read and print simulation parameters
    TITLE("Reading the simulation parameters");
    Params params(&smpi,vector<string>(argv + 1, argv + argc));
    OpenPMDparams openPMD(params);
    
    // Need to move it here because of domain decomposition need in smpi->init(_patch_count)
    //     abstraction of Hilbert curve
    VectorPatch vecPatches( params );

    // Initialize MPI environment with simulation parameters
    TITLE("Initializing MPI");
    smpi.init(params, vecPatches.domain_decomposition_);
    
    // Create timers
    Timers timers(&smpi);

    // Print in stdout MPI, OpenMP, patchs parameters
    params.print_parallelism_params(&smpi);

    TITLE("Initializing the restart environment");
    Checkpoint checkpoint(params, &smpi);

    // ------------------------------------------------------------------------
    // Initialize the simulation times time_prim at n=0 and time_dual at n=+1/2
    // Update in "if restart" if necessary
    // ------------------------------------------------------------------------

    // time at integer time-steps (primal grid)
    double time_prim = 0;
    // time at half-integer time-steps (dual grid)
    double time_dual = 0.5 * params.timestep;

    // -------------------------------------------
    // Declaration of the main objects & operators
    // -------------------------------------------
    // --------------------
    // Define Moving Window
    // --------------------
    TITLE("Initializing moving window");
    SimWindow* simWindow = new SimWindow(params);

    // ------------------------------------------------------------------------
    // Init nonlinear inverse Compton scattering
    // ------------------------------------------------------------------------
    RadiationTables RadiationTables;

    // ------------------------------------------------------------------------
    // Create MultiphotonBreitWheelerTables object for multiphoton
    // Breit-Wheeler pair creation
    // ------------------------------------------------------------------------
    MultiphotonBreitWheelerTables MultiphotonBreitWheelerTables;

    // ---------------------------------------------------
    // Initialize patches (including particles and fields)
    // ---------------------------------------------------
    TITLE("Initializing particles & fields");

    if( smpi.test_mode ) {
        execute_test_mode( vecPatches, &smpi, simWindow, params, checkpoint, openPMD );
        return 0;
    }

    // reading from dumped file the restart values
    if (params.restart) {

        // smpi.patch_count recomputed in readPatchDistribution
        checkpoint.readPatchDistribution( &smpi, simWindow );
        // allocate patches according to smpi.patch_count
        vecPatches = PatchesFactory::createVector(params, &smpi, openPMD, checkpoint.this_run_start_step+1, simWindow->getNmoved());
        // vecPatches data read in restartAll according to smpi.patch_count
        checkpoint.restartAll( vecPatches, &smpi, simWindow, params, openPMD);

        // time at integer time-steps (primal grid)
        time_prim = checkpoint.this_run_start_step * params.timestep;
        // time at half-integer time-steps (dual grid)
        time_dual = (checkpoint.this_run_start_step +0.5) * params.timestep;

        // ---------------------------------------------------------------------
        // Init and compute tables for radiation effects
        // (nonlinear inverse Compton scattering)
        // ---------------------------------------------------------------------
        RadiationTables.initParams(params);
        RadiationTables.compute_tables(params,&smpi);
        RadiationTables.output_tables(&smpi);

        // ---------------------------------------------------------------------
        // Init and compute tables for multiphoton Breit-Wheeler pair creation
        // ---------------------------------------------------------------------
        MultiphotonBreitWheelerTables.initialization(params);
        MultiphotonBreitWheelerTables.compute_tables(params,&smpi);
        MultiphotonBreitWheelerTables.output_tables(&smpi);

        TITLE("Initializing diagnostics");
        vecPatches.initAllDiags( params, &smpi );

    } else {

        vecPatches = PatchesFactory::createVector(params, &smpi, openPMD, 0);

        // Initialize the electromagnetic fields
        // -------------------------------------
        vecPatches.computeCharge();
        vecPatches.sumDensities(params, time_dual, timers, 0, simWindow);

        // ---------------------------------------------------------------------
        // Init and compute tables for radiation effects
        // (nonlinear inverse Compton scattering)
        // ---------------------------------------------------------------------
        RadiationTables.initParams(params);
        RadiationTables.compute_tables(params,&smpi);
        RadiationTables.output_tables(&smpi);

        // ---------------------------------------------------------------------
        // Init and compute tables for multiphoton Breit-Wheeler pair decay
        // ---------------------------------------------------------------------
        MultiphotonBreitWheelerTables.initialization(params);
        MultiphotonBreitWheelerTables.compute_tables(params,&smpi);
        MultiphotonBreitWheelerTables.output_tables(&smpi);

        // Apply antennas
        // --------------
        vecPatches.applyAntennas(0.5 * params.timestep);

        // Init electric field (Ex/1D, + Ey/2D)
        if (!vecPatches.isRhoNull(&smpi) && params.solve_poisson == true) {
            TITLE("Solving Poisson at time t = 0");
            vecPatches.solvePoisson( params, &smpi );
        }

        TITLE("Applying external fields at time t = 0");
        vecPatches.applyExternalFields();

        vecPatches.dynamics(params, &smpi, simWindow, RadiationTables,
                            MultiphotonBreitWheelerTables, time_dual, timers, 0);

        vecPatches.sumDensities(params, time_dual, timers, 0, simWindow );

        vecPatches.finalize_and_sort_parts(params, &smpi, simWindow,
            RadiationTables,MultiphotonBreitWheelerTables, 
            time_dual, timers, 0);

        TITLE("Initializing diagnostics");
        vecPatches.initAllDiags( params, &smpi );
        TITLE("Running diags at time t = 0");
        vecPatches.runAllDiags(params, &smpi, 0, timers, simWindow);
    }

    TITLE("Species creation summary");
    vecPatches.printNumberOfParticles( &smpi );

    timers.reboot();


    Domain domain( params ); 
    unsigned int global_factor(1);
    //#ifdef _PICSAR
    for ( unsigned int iDim = 0 ; iDim < params.nDim_field ; iDim++ )
        global_factor *= params.global_factor[iDim];
    // Force temporary usage of double grids, even if global_factor = 1
    //    especially to compare solvers
    //if (global_factor!=1) {
        domain.build( params, &smpi, vecPatches, openPMD );
    //}
    //#endif

    timers.global.reboot();
    
    // ------------------------------------------------------------------------
    // Check memory consumption & expected disk usage
    // ------------------------------------------------------------------------
    TITLE("Memory consumption");
    vecPatches.check_memory_consumption( &smpi );
    
    TITLE("Expected disk usage (approximate)");
    vecPatches.check_expected_disk_usage( &smpi, params, checkpoint );
    
    // ------------------------------------------------------------------------
    // check here if we can close the python interpreter
    // ------------------------------------------------------------------------
    TITLE("Cleaning up python runtime environement");
    params.cleanup(&smpi);

/*tommaso
    // save latestTimeStep (used to test if we are at the latest timestep when running diagnostics at run's end)
    unsigned int latestTimeStep=checkpoint.this_run_start_step;
*/
    // ------------------------------------------------------------------
    //                     HERE STARTS THE PIC LOOP
    // ------------------------------------------------------------------

    // New_DD : new distrib
    //smpi.patch_count[0] = 10; // +2
    //smpi.patch_count[1] = 27; // +5 -2
    //smpi.patch_count[2] = 21; // +2 -5
    //smpi.patch_count[3] = 6;  //    -2
    //vecPatches.createPatches(params, &smpi, simWindow);
    //vecPatches.exchangePatches(&smpi, params);
    //vecPatches.lastIterationPatchesMoved = 0;

    // New_DD : non local
    domain.identify_additional_patches( &smpi, vecPatches );
    //if (smpi.getRank()==0) {
    //    domain.additional_patches_ranks[0] = 1;
    //    domain.additional_patches_ranks[1] = 1;
    //}
    //else if(smpi.getRank()==1) {
    //    domain.additional_patches_ranks[0] = 2;
    //    domain.additional_patches_ranks[1] = 2;
    //    domain.additional_patches_ranks[2] = 2;
    //    domain.additional_patches_ranks[3] = 2;
    //    domain.additional_patches_ranks[4] = 2;
    //}
    //else if(smpi.getRank()==2) {
    //    domain.additional_patches_ranks[0] = 3;
    //    domain.additional_patches_ranks[1] = 3;
    //}
    if (smpi.getRank()==0) {
        for (int idx=0 ; idx<4 ; idx++)
            domain.additional_patches_ranks[idx] = 3;
        for (int idx=4 ; idx<8 ; idx++)
            domain.additional_patches_ranks[idx] = 2;
        for (int idx=8 ; idx<12 ; idx++)
            domain.additional_patches_ranks[idx] = 1;
    }
    else if(smpi.getRank()==1) {
        for (int idx=0 ; idx<8 ; idx++)
            domain.additional_patches_ranks[idx] = 2;
    }
    else if(smpi.getRank()==3) {
        for (int idx=0 ; idx<8 ; idx++)
            domain.additional_patches_ranks[idx] = 2;
    }

    domain.identify_missing_patches( &smpi, vecPatches, params );
    //if (smpi.getRank()==1) {
    //    domain.missing_patches_ranks[0] = 0;
    //    domain.missing_patches_ranks[1] = 0;
    //}
    //else if(smpi.getRank()==2) {
    //    domain.missing_patches_ranks[0] = 1;
    //    domain.missing_patches_ranks[1] = 1;
    //    domain.missing_patches_ranks[2] = 1;
    //    domain.missing_patches_ranks[3] = 1;
    //    domain.missing_patches_ranks[4] = 1;
    //}
    //else if(smpi.getRank()==3) {
    //    domain.missing_patches_ranks[0] = 2;
    //    domain.missing_patches_ranks[1] = 2;
    //}
    if (smpi.getRank()==1) {
        for (int idx=0 ; idx<4 ; idx++)
            domain.missing_patches_ranks[idx] = 0;
    }
    else if(smpi.getRank()==2) {
        for (int idx=0 ; idx<4 ; idx++)
            domain.missing_patches_ranks[idx] = 0;
        for (int idx=4 ; idx<12 ; idx++)
            domain.missing_patches_ranks[idx] = 1;
        for (int idx=12 ; idx<20 ; idx++)
            domain.missing_patches_ranks[idx] = 3;
    }
    else if(smpi.getRank()==3) {
        for (int idx=0 ; idx<4 ; idx++)
            domain.missing_patches_ranks[idx] = 0;
    }
    MPI_Barrier( MPI_COMM_WORLD );
    //return 0;


    TITLE("Time-Loop started: number of time-steps n_time = " << params.n_time);
    if ( smpi.isMaster() ) params.print_timestep_headers();

    #pragma omp parallel shared (time_dual,smpi,params, vecPatches, domain, simWindow, checkpoint)
    {
        
        unsigned int itime=checkpoint.this_run_start_step+1;
        while ( (itime <= params.n_time) && (!checkpoint.exit_asap) ) {
            
            // calculate new times
            // -------------------
            #pragma omp single
            {
                time_prim += params.timestep;
                time_dual += params.timestep;
            }
            // apply collisions if requested
            vecPatches.applyCollisions(params, itime, timers);
            
            // (1) interpolate the fields at the particle position
            // (2) move the particle
            // (3) calculate the currents (charge conserving method)
            vecPatches.dynamics(params, &smpi, simWindow, RadiationTables,
                                MultiphotonBreitWheelerTables,
                                time_dual, timers, itime);
            
            // Sum densities
            vecPatches.sumDensities(params, time_dual, timers, itime, simWindow );
            
            // apply currents from antennas
            vecPatches.applyAntennas(time_dual);
            
            // solve Maxwell's equations
            //#ifndef _PICSAR
            //// Force temporary usage of double grids, even if global_factor = 1
            ////    especially to compare solvers           
            ////if ( global_factor==1 )
            //{
            //    if( time_dual > params.time_fields_frozen ) {
            //        vecPatches.solveMaxwell( params, simWindow, itime, time_dual, timers );
            //    }
            //}
            //#else
            // Force temporary usage of double grids, even if global_factor = 1
            //    especially to compare solvers           
            //if ( global_factor!=1 )
            MPI_Barrier( MPI_COMM_WORLD );
            //MESSAGE("Before domain");
            {
                if( time_dual > params.time_fields_frozen ) {
                    MPI_Barrier( MPI_COMM_WORLD );
                    //MESSAGE("Before to global");
                    SyncCartesianPatch::patchedToCartesian( vecPatches, domain, params, &smpi, timers, itime );
                    MPI_Barrier( MPI_COMM_WORLD );
                    //MESSAGE("Before Maxwell");
                    domain.solveMaxwell( params, simWindow, itime, time_dual, timers );
                    //MESSAGE("Before to patchs");
                    SyncCartesianPatch::cartesianToPatches( domain, vecPatches, params, &smpi, timers, itime );
                }
            }
            //MESSAGE("After domain");
            //#endif

            vecPatches.finalize_and_sort_parts(params, &smpi, simWindow, RadiationTables,
                                               MultiphotonBreitWheelerTables,
                                               time_dual, timers, itime);
            vecPatches.finalize_sync_and_bc_fields(params, &smpi, simWindow, time_dual, timers, itime);

            // call the various diagnostics
            vecPatches.runAllDiags(params, &smpi, itime, timers, simWindow);
            
            timers.movWindow.restart();
            simWindow->operate(vecPatches, &smpi, params, itime, time_dual);
            timers.movWindow.update();
            
            // ----------------------------------------------------------------------
            // Validate restart  : to do
            // Restart patched moving window : to do
            #pragma omp master
            checkpoint.dump(vecPatches, itime, &smpi, simWindow, params);
            #pragma omp barrier
            // ----------------------------------------------------------------------
            
            
            if( params.has_load_balancing ) {
                if( params.load_balancing_time_selection->theTimeIsNow(itime) ) {
                    timers.loadBal.restart();
                    #pragma omp single
                    vecPatches.load_balance( params, time_dual, &smpi, simWindow, itime );
                    timers.loadBal.update( params.printNow( itime ) );
                }
            }
            
            // print message at given time-steps
            // --------------------------------
            if ( smpi.isMaster() &&  params.printNow( itime ) )
                params.print_timestep(itime, time_dual, timers.global); //contain a timer.update !!!
            
            if ( params.printNow( itime ) ) {
                #pragma omp master
                timers.consolidate( &smpi );
                #pragma omp barrier
            }

            itime++;
            
        }//END of the time loop

    } //End omp parallel region

    smpi.barrier();

    // ------------------------------------------------------------------
    //                      HERE ENDS THE PIC LOOP
    // ------------------------------------------------------------------
    TITLE("End time loop, time dual = " << time_dual);
    timers.global.update();

    TITLE("Time profiling : (print time > 0.001%)");
    timers.profile(&smpi);

/*tommaso
    // ------------------------------------------------------------------
    //                      Temporary validation diagnostics
    // ------------------------------------------------------------------

    if (latestTimeStep==params.n_time)
        vecPatches.runAllDiags(params, smpi, &diag_flag, params.n_time, timer, simWindow);
*/

    // ------------------------------
    //  Cleanup & End the simulation
    // ------------------------------
    if (global_factor!=1) 
        domain.clean();
    vecPatches.close( &smpi );
    smpi.barrier(); // Don't know why but sync needed by HDF5 Phasespace managment
    delete simWindow;
    PyTools::closePython();
    TITLE("END");

    return 0;

}//END MAIN

// ---------------------------------------------------------------------------------------------------------------------
//                                               END MAIN CODE
// ---------------------------------------------------------------------------------------------------------------------


int execute_test_mode( VectorPatch &vecPatches, SmileiMPI* smpi, SimWindow* simWindow, Params &params, Checkpoint &checkpoint, OpenPMDparams& openPMD )
{
    int itime = 0;
    int moving_window_movement = 0;
    
    if (params.restart) {
        checkpoint.readPatchDistribution( smpi, simWindow );
        itime = checkpoint.this_run_start_step+1;
        moving_window_movement = simWindow->getNmoved();
    }
    
    vecPatches = PatchesFactory::createVector(params, smpi, openPMD, itime, moving_window_movement );
    
    if (params.restart)
        checkpoint.restartAll( vecPatches, smpi, simWindow, params, openPMD);
    
    if( params.print_expected_disk_usage ) {
        TITLE("Expected disk usage (approximate)");
        vecPatches.check_expected_disk_usage( smpi, params, checkpoint );
    }
    
    // If test mode enable, code stops here
    TITLE("Cleaning up python runtime environement");
    params.cleanup(smpi);
    delete simWindow;
    PyTools::closePython();
    TITLE("END TEST MODE");

    return 0;
}
