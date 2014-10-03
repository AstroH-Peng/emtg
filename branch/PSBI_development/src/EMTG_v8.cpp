//============================================================================
// Name        : EMTG_v8.cpp
// Author      : Jacob Englander
// Version     :
// Copyright   : 
// Description : Main launch function for EMTG_v8
// Description : EMTG_v8 is a generic optimizer that hands MGA, MGADSM, MGALT, and FBLT mission types
//============================================================================

#include <iostream>
#include <fstream>
#include <sstream>

#include "missionoptions.h"
#include "mission.h"
#include "outerloop_NSGAII.h"
#include "outerloop_SGA.h"


#include "universe.h"
#include "body.h"

#include "boost/filesystem.hpp"
#include "boost/filesystem/fstream.hpp"
#include "boost/date_time.hpp"
#include "boost/date_time/local_time/local_date_time.hpp"
#include "boost/lexical_cast.hpp"

#include "SpiceUsr.h"



#ifdef EMTG_MPI
#include <boost/mpi/environment.hpp>
#include <boost/mpi/communicator.hpp>
#endif

using namespace std;
using namespace boost::filesystem;
using namespace boost::gregorian;
using namespace boost::posix_time;

#ifdef _STONEAGECplusplus
#include <execinfo.h>
#include <signal.h>
void handler(int sig) {
  void *array[10];
  size_t size;

  // get void*'s for all entries on the stack
  size = backtrace(array, 10);

  // print out all the frames to stderr
  fprintf(stderr, "Error: signal %d:\n", sig);
  backtrace_symbols_fd(array, size, STDERR_FILENO);
  exit(1);
}
#endif

int main(int argc, char* argv[]) 
{
	//delete the fort if present
#ifndef _STONEAGECplusplus
	fs::path fort(L"fort.1"); 
	fs::remove(fort);
#else
	signal(SIGSEGV, handler);
#endif


	cout << "program starting" << endl;

	//parse the options file
	string options_file_name;
	if (argc == 1)
		options_file_name = "default.emtgopt";
	else if (argc == 2) 
		options_file_name.assign(argv[1]);

	cout << options_file_name << endl;

	EMTG::missionoptions options(options_file_name);
	cout << options.error_message << endl;
	if (!(options.file_status == 0))
	{
		cout << "Aborting program run." << endl;
		cin.ignore();
		return 0;
	}

	//if we are running in parallel, start MPI
#ifdef EMTG_MPI
	boost::mpi::environment MPIEnvironment;
	boost::mpi::communicator MPIWorld;
#endif


	//create a working directory for the problem
	//only the head node should do this, then broadcast the folder name to everyone else
	//*****************************************************************
#ifdef EMTG_MPI
	if (MPIWorld.rank() == 0)
#endif
	{
		if (options.override_working_directory)
		{
			options.working_directory = options.forced_working_directory;
		}
		else
		{
			ptime now = second_clock::local_time();
			std::stringstream timestream;
			timestream << static_cast<int>(now.date().month()) << now.date().day() << now.date().year() << "_" << now.time_of_day().hours() << now.time_of_day().minutes() << now.time_of_day().seconds();


			//define a new working directory
			options.working_directory = "..//EMTG_v8_results//" + options.mission_name + "_" + timestream.str();

		} //end if not override working directory

		if (!(options.run_outerloop == 2))
		{
			//create the working directory
			try
			{
				path p(options.working_directory);
				boost::filesystem::create_directories(p);
			}
			catch (std::exception &e)
			{
				std::cerr << "Error " << e.what() << ": Directory creation failed" << std::endl;
			}

			//print the options file to the new directory
			options.print_options_file(options.working_directory + "//" + options.mission_name + ".emtgopt");
		}
	} //end working directory creation and options file printing for head node

	//broadcast the working directory to the other nodes
#ifdef EMTG_MPI
	boost::mpi::broadcast(MPIWorld, options.working_directory, 0);
#endif
	
	
	//load all ephemeris data if using SPICE
	vector<fs::path> SPICE_files_initial;
    vector<fs::path> SPICE_files_not_required;
    vector<fs::path> SPICE_files_required;
    vector<int> SPICE_bodies_required;
	string filestring;
	if (options.ephemeris_source == 1)
	{	
        //load all BSP files
		EMTG::filesystem::get_all_files_with_extension(fs::path(options.universe_folder + "/ephemeris_files/"), ".bsp", SPICE_files_initial);

		for (size_t k = 0; k < SPICE_files_initial.size(); ++k)
		{
			filestring = options.universe_folder + "/ephemeris_files/" + SPICE_files_initial[k].string();
			furnsh_c(filestring.c_str());
		}

		//SPICE reference frame kernel
		string leapsecondstring = options.universe_folder + "/ephemeris_files/" + options.SPICE_leap_seconds_kernel;
		string referenceframestring = options.universe_folder + "/ephemeris_files/" + options.SPICE_reference_frame_kernel;
		furnsh_c(leapsecondstring.c_str());
		furnsh_c(referenceframestring.c_str());

		//disable SPICE errors. This is because we can, and will often, go off the edge of an ephemeris file.
		errprt_c((SpiceChar*)"SET", 100, "NONE");
		erract_c((SpiceChar*)"SET", 100, "RETURN");
	}

	//create a vector of universes for each journey
	boost::ptr_vector<EMTG::Astrodynamics::universe> TheUniverse;
	options.TU = 0;
	for (int j = 0; j < options.number_of_journeys; ++j)
	{
		TheUniverse.push_back(new EMTG::Astrodynamics::universe(j, options.universe_folder + "//" + options.journey_central_body[j] + ".emtg_universe", &options));
		stringstream universenamestream;

		universenamestream << options.journey_central_body[j] + "_Journey_" << j << ".universe_output";

		if (TheUniverse[j].TU > options.TU)
			options.TU = TheUniverse[j].TU;
	}

    //now unload ephemeris files that we don't need
    if (options.ephemeris_source == 1)
    {
        //make a list of all of the bodies that we actually need by SPICE ID, make sure all planet barycenters are included
        //always include the sun and all barycenters
        for (size_t b = 0; b < 11; ++b)
            SPICE_bodies_required.push_back(b);

        //everything else is organized by journey
        for (size_t j = 0; j < options.number_of_journeys; ++j)
        {
            //include the journey's central body
            SPICE_bodies_required.push_back(TheUniverse[j].central_body_SPICE_ID);
            //include destination bodies
            SPICE_bodies_required.push_back(TheUniverse[j].bodies[options.destination_list[j][0] - 1].spice_ID);
            SPICE_bodies_required.push_back(TheUniverse[j].bodies[options.destination_list[j][1] - 1].spice_ID);
            //include flyby sequence bodies
            for (size_t s = 0; s < options.sequence_input.size(); ++s)
            {
                for (size_t b = 0; b < options.sequence_input[s][j].size(); ++b)
                    if (options.sequence_input[s][j][b] > 0)
                        SPICE_bodies_required.push_back(TheUniverse[j].bodies[options.sequence_input[s][j][b] - 1].spice_ID);
            }
            //include perturbation bodies
            if (options.perturb_thirdbody)
            {
                for (size_t b = 0; b < options.journey_perturbation_bodies[j].size(); ++b)
                    SPICE_bodies_required.push_back(TheUniverse[j].bodies[options.journey_perturbation_bodies[j][b] - 1].spice_ID);
            }

            if (options.run_outerloop > 0)
            {
                //include CANDIDATE destination bodies
                if (options.outerloop_vary_journey_destination[j])
                {
                    for (size_t b = 0; b < options.outerloop_journey_destination_choices[j].size(); ++b)
                        SPICE_bodies_required.push_back(TheUniverse[j].bodies[options.outerloop_journey_destination_choices[j][b] - 1].spice_ID);
                }
                //include CANDIDATE flyby sequence bodies
                if (options.outerloop_vary_journey_flyby_sequence[j])
                {
                    for (size_t b = 0; b < options.outerloop_journey_flyby_sequence_choices[j].size(); ++b)
                        if (options.outerloop_journey_flyby_sequence_choices[j][b] > 0)
                            SPICE_bodies_required.push_back(TheUniverse[j].bodies[options.outerloop_journey_flyby_sequence_choices[j][b] - 1].spice_ID);
                }
            }
        }

        //sort and remove duplicates
        std::sort(SPICE_bodies_required.begin(), SPICE_bodies_required.end());
        SPICE_bodies_required.erase(std::unique(SPICE_bodies_required.begin(), SPICE_bodies_required.end()), SPICE_bodies_required.end());

        //for now do not unload anything
        SPICE_files_required = SPICE_files_initial;
        //make a list of kernels that we do need and a list of kernels that we do need
        //do this by looping over every kernel and determining if we are going to use it or not
        /*
        for (size_t kernel = 0; kernel < SPICE_files_initial.size(); ++kernel)
        {
            //stupid SPICE preprocessor macros when you should be using a class
            //from http://naif.jpl.nasa.gov/pub/naif/toolkit_docs/C/cspice/spkobj_c.html
            SPICEINT_CELL(bodies_in_this_kernel, 1000);

            spkobj_c(SPICE_files_initial[kernel].string().data(), &bodies_in_this_kernel);

            //now check "bodies_in_this_kernel" to see if it contains useful bodies
            //if it does, add it to "SPICE_files_required"
            //otherwise unload it
            bool useful_kernel = false;
            for (size_t kernelbody = 0; kernelbody < card_c(&bodies_in_this_kernel); ++kernelbody)
            {
                for (size_t requiredbody = 0; requiredbody < SPICE_bodies_required.size(); ++requiredbody)
                {
                    int thisbody;
                    SPICE_CELL_GET_I(&bodies_in_this_kernel, kernelbody, &thisbody);
                    if (SPICE_bodies_required[requiredbody] == thisbody)
                    {
                        useful_kernel = true;
                        break;
                    }
                }
                if (useful_kernel)
                    break;
            }
            if (useful_kernel)
                SPICE_files_required.push_back(SPICE_files_initial[kernel]);
            else
            {
                filestring = options.universe_folder + "ephemeris_files/" + SPICE_files_initial[kernel].string();
                unload_c(filestring.c_str());
            }
        }
        */
    }
    

	//*****************************************************************


	//next, it is time to start the outer-loop

	if (options.run_outerloop == 1 && options.outerloop_objective_function_choices.size() == 1)
	{
		//Step 1: instantiate an SGA object
#ifdef EMTG_MPI
		GeneticAlgorithm::outerloop_SGA SGA(options, &MPIEnvironment, &MPIWorld);
#else
		GeneticAlgorithm::outerloop_SGA SGA(options);
#endif

		//Step 2: generate a random population
		SGA.set_populationsize(options.outerloop_popsize);
		SGA.set_mutationrate(options.outerloop_mu);
		SGA.set_max_generations(options.outerloop_genmax);
		SGA.set_CR(options.outerloop_CR);
		SGA.set_elitecount(options.outerloop_elitecount);
		SGA.set_tournament_size(options.outerloop_tournamentsize);
		if (options.outerloop_warmstart && !(options.outerloop_warm_archive == "none"))
			SGA.read_archive(options.outerloop_warm_archive);

		if (options.outerloop_warmstart && !(options.outerloop_warm_population == "none"))
			SGA.readpop(options.outerloop_warm_population);
		else
			SGA.generatepop();

		SGA.startclock();
		SGA.evolve(options, TheUniverse);

		//Step 3: write out the population
		SGA.writepop(options.working_directory + "//SGA_final_population.SGA");

		//Step 4: write out the archive
		SGA.write_archive(options.working_directory + "//SGA_archive.SGA");
	}
	else if (options.run_outerloop == 1 && options.outerloop_objective_function_choices.size() > 1)
	{
		//Step 1: instantiate an NSGA-II object
#ifdef EMTG_MPI
		GeneticAlgorithm::outerloop_NSGAII NSGAII(options, &MPIEnvironment, &MPIWorld);
#else
		GeneticAlgorithm::outerloop_NSGAII NSGAII(options);
#endif

		//Step 2: generate a random population
		NSGAII.set_populationsize(options.outerloop_popsize);
		NSGAII.set_mutationrate(options.outerloop_mu);
		NSGAII.set_max_generations(options.outerloop_genmax);
		NSGAII.set_CR(options.outerloop_CR);
		if (options.outerloop_warmstart && !(options.outerloop_warm_archive == "none"))
			NSGAII.read_archive(options.outerloop_warm_archive);

		if (options.outerloop_warmstart && !(options.outerloop_warm_population == "none"))
			NSGAII.readpop(options.outerloop_warm_population);
		else
			NSGAII.generatepop();

		NSGAII.startclock();
		NSGAII.evolve(options, TheUniverse);

		//Step 3: write out the population
		NSGAII.writepop(options.working_directory + "//NSGAII_final_population.NSGAII");

		//Step 4: write out the archive
		NSGAII.write_archive(options.working_directory + "//NSGAII_archive.NSGAII");
	}
	else
	{
		//set up a batch output file
		string outputfilestring = options.working_directory + "//" + options.mission_name + "_batch_summary.emtgbatch";
		std::ofstream outputfile(outputfilestring.c_str(), ios::trunc);
		outputfile.width(30); outputfile << left << "Sequence";
		outputfile.width(3); outputfile << " | ";

		switch (options.objective_type)
		{
		case 0: //deltaV
			outputfile.width(17); outputfile << left << "deltaV (km/s)";
			break;
		case 2:
			outputfile.width(17); outputfile << left << "delivered mass (kg)";
			break;
		}
		
		outputfile << endl;
		for (int k = 0; k < 50; ++k)
		outputfile << "-";
		outputfile << endl;
		outputfile.close();

		//run the trial outer-loop vectors

		for (int trial = 0; trial < options.number_of_trial_sequences; ++trial)
		{
			//first we need to create an outer-loop decision vector based on the parameters from the options file
			vector<int> Xouterloop_trial;

			//if we have specified the mission type (all MGA, MGA-DSM, MGA-LT, FBLT, FBLT-S, MGA-NDSM) then it only takes one decision variable to encode a phase
			//if we have NOT specified the mission type, it takes two decision variables to encode a phase
			int phase_encode_length = (options.mission_type > 6 ? 2 : 1);
			
			for (int j = 0; j < options.number_of_journeys; ++j)
			{
				for (int p = 0; p < options.max_phases_per_journey; ++p)
				{
					Xouterloop_trial.push_back(options.sequence_input[trial][j][p]);

					//encode the phase type in the decision vector only if we are allowing the outer-loop to choose phase type
					if (options.sequence_input[trial][j][p] > 0 && phase_encode_length == 2)
							Xouterloop_trial.push_back(options.phase_type_input[j][p]);
				}

				if (phase_encode_length == 2)
					Xouterloop_trial.push_back(options.phase_type_input[j][options.phase_type_input[j].size() - 1]);

				options.number_of_phases = options.number_of_phases_input[trial];
			}

			//next, instantiate and optimize a problem object
			try
			{
				if (options.problem_type == 0) //regular EMTG missions
				{
					EMTG::mission TrialMission(&Xouterloop_trial[0], &options, TheUniverse, 0, 0);

					//and now, as a demo, print the mission tree
					TrialMission.output_mission_tree(options.working_directory + "//" + TrialMission.options.mission_name + "_" + TrialMission.options.description + "_missiontree.emtgtree");

					//copy the appropriate trialX, if necessary
					if (options.run_inner_loop == 0 || options.run_inner_loop == 4)
					{
						TrialMission.options.current_trialX = options.trialX[trial];

						//if we are interpolating an initial guess to change the resolution
						if (options.interpolate_initial_guess && options.run_inner_loop > 0)
						{
							TrialMission.interpolate(Xouterloop_trial.data(), TrialMission.options.current_trialX);
						}

						//convert coordinate systems if applicable
						if (((options.run_inner_loop == 2 && options.seed_MBH) || options.run_inner_loop == 4) && (options.mission_type == 2 || options.mission_type == 3))
						{
							if (options.control_coordinate_system == 1 && options.initial_guess_control_coordinate_system == 0)
								TrialMission.convert_cartesian_solution_to_polar(TrialMission.options.current_trialX);
							else if (options.control_coordinate_system == 0 && options.initial_guess_control_coordinate_system == 1)
								TrialMission.convert_polar_solution_to_cartesian(TrialMission.options.current_trialX);
						}	
					}
					else if (options.run_inner_loop == 2)
					{
						TrialMission.options.current_trialX = options.trialX[trial];

						if (options.interpolate_initial_guess && options.seed_MBH)
							TrialMission.interpolate(Xouterloop_trial.data(), options.current_trialX);

						//convert coordinate systems if applicable
						if (((options.run_inner_loop == 2 && options.seed_MBH) || options.run_inner_loop == 4) && (options.mission_type == 2 || options.mission_type == 3))
						{
							if (options.control_coordinate_system == 1 && options.initial_guess_control_coordinate_system == 0)
								TrialMission.convert_cartesian_solution_to_polar(TrialMission.options.current_trialX);
							else if (options.control_coordinate_system == 0 && options.initial_guess_control_coordinate_system == 1)
								TrialMission.convert_polar_solution_to_cartesian(TrialMission.options.current_trialX);
						}
					}

					

					//evaluate the mission
					TrialMission.optimize();

					//output the mission
					TrialMission.output();

                    //output forward-integrated ephemeris if appropriate
                    if (options.generate_forward_integrated_ephemeris && options.mission_type == 3)
                        TrialMission.write_ephemeris_file();

					//output GMAT files
					//temporarily we can't output MGA or MGA-DSM missions
					if (options.mission_type > 1 && options.create_GMAT_script)
					{
						TrialMission.output_GMAT_preamble();
						TrialMission.output_GMAT_mission();
					}

					if (options.generate_initial_guess_file)
						TrialMission.create_initial_guess(options.mission_type_for_initial_guess_file, false);

					//store the results in a database file
					string outputfilestring = options.working_directory + "//" + options.mission_name + "_batch_summary.emtgbatch";
					std::ofstream outputfile(outputfilestring.c_str(), ios::app);
					outputfile.width(30); outputfile << left << TrialMission.options.description;
					outputfile.width(3); outputfile << " | ";

					switch (options.objective_type)
					{
					case 0: //deltaV
						outputfile.width(17); outputfile << left << TrialMission.F[0];
						break;
					case 2:
						outputfile.width(17); outputfile << left << TrialMission.F[0] * -options.maximum_mass;
						break;
					}
					outputfile << endl;
					outputfile.close();
				}
			}
			catch (std::exception &e)
			{
				cout << "Error " << e.what() << ": Failure to run inner-loop solver" << endl;
			}
		}//end loop over trial sequences


	}

	//unload SPICE

	if (options.ephemeris_source == 1)
	{
		for (size_t k = 0; k < SPICE_files_required.size(); ++k)
		{
			filestring = options.universe_folder + "ephemeris_files/" + SPICE_files_required[k].string();
			unload_c(filestring.c_str());
		}

		unload_c((options.universe_folder + "ephemeris_files/" + options.SPICE_leap_seconds_kernel).c_str());
		unload_c((options.universe_folder + "ephemeris_files/" + options.SPICE_reference_frame_kernel).c_str());
	}

	std::cout << "EMTG run complete." << std::endl;
#ifndef BACKGROUND_MODE
	std::cout << "Press enter to close window." << std::endl;
	std::cin.ignore();
#endif

	return 0;
}
