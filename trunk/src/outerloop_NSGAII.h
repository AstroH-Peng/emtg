//outer-loop NSGA-II
//written to solve systems optimization as formulated by J. Englander, M. Vavrina, and D. Ellison
//collaborative effort by J. Englander and M. Vavrina based on A. Ghosh's abstract GA spec and M. Vavrina's NSGA-II spec

#include <vector>
#include <string>
#include <ctime>

#include "missionoptions.h"
#include "universe.h"
#include "EMTG_outerloop_solution.h"

#include "boost/random/uniform_int.hpp"
#include "boost/random/uniform_real.hpp"
#include "boost/random/mersenne_twister.hpp"
#include "boost/ptr_container/ptr_vector.hpp"

#ifdef EMTG_MPI
#include "boost/serialization/string.hpp"
#include "boost/serialization/vector.hpp"
#include <boost/mpi/environment.hpp>
#include <boost/mpi/communicator.hpp>
#include <boost/mpi/collectives.hpp>
#endif

#ifndef OUTERLOOP_NSGAII
#define OUTERLOOP_NSGAII

namespace GeneticAlgorithm
{
	class outerloop_NSGAII
	{
		private:
			//methods
			std::vector< EMTG_outerloop_solution >  select(const std::vector<EMTG_outerloop_solution> &parent_population);
			std::vector< EMTG_outerloop_solution >  crossover_uniformInt(std::vector<EMTG_outerloop_solution> &parent_pool);
			void mutate(std::vector< EMTG_outerloop_solution > &population);
			void non_dominated_sort(std::vector<EMTG_outerloop_solution> &population);
			void assign_crowding_distance(std::vector< EMTG_outerloop_solution > &local_front);
			void filter_population();

			//distributions
			std::vector< boost::uniform_int<> > random_integer;

			//fields
			boost::mt19937 RNG;
    		boost::uniform_int<> IntegerDistribution;
    		boost::uniform_real<> DoubleDistribution;
			double CR; //crossover rate
			double mu; //mutation rate
			int tournamentsize; //tournament size for selection
			int stallmax; //maximum number of stall generations
			int genmax; //maximum number of generations
			int popsize; //number of individuals in the population
			int current_generation;
			int stall_generation;
			int number_of_objectives;
			std::vector<std::string> objective_descriptions;
			std::vector<double> tolfit; //tolerance for each fitness function
			std::vector< EMTG_outerloop_solution > this_generation, last_generation;
			std::vector< EMTG_outerloop_solution > parent_population, children_population, parent_pool;
			std::vector< std::vector < EMTG_outerloop_solution > > nondominated_fronts; // local non-dominated front levels for a population of solutions
			std::vector< EMTG_outerloop_solution > archive_of_solutions;
			std::vector<int> Xupperbounds, Xlowerbounds;
			std::vector<std::string> Xdescriptions;

			time_t tstart, tfinish;

			//communication code - pointers to World and Environment
#ifdef EMTG_MPI
			boost::mpi::environment* MPIEnvironment;
			boost::mpi::communicator* MPIWorld;
#endif

		public:
			//constructor
			outerloop_NSGAII(const EMTG::missionoptions& options); //seedless
			outerloop_NSGAII(int boost_random_seed, const EMTG::missionoptions& options);
#ifdef EMTG_MPI
			outerloop_NSGAII(const EMTG::missionoptions& options, boost::mpi::environment* MPIEnvironment, boost::mpi::communicator* MPIWorld);
			outerloop_NSGAII(int boost_random_seed, const EMTG::missionoptions& options, boost::mpi::environment* MPIEnvironment, boost::mpi::communicator* MPIWorld);
#endif

			//methods
			void evaluatepop(const EMTG::missionoptions& options, const boost::ptr_vector<EMTG::Astrodynamics::universe>& Universe);
			void generatepop();
			void readpop(std::string filename);
			void writepop(std::string filename);
			void read_archive(std::string filename);
			void write_archive(std::string filename);
			void evolve(const EMTG::missionoptions& options, const boost::ptr_vector<EMTG::Astrodynamics::universe>& Universe);
			void reset();
			void calcbounds(const EMTG::missionoptions& options);
			void startclock() {this->tstart = time(NULL);}

			void supply_pop(std::vector< EMTG_outerloop_solution > const & input_population);
			void supply_archive(std::vector< EMTG_outerloop_solution > const & input_archive);
			const std::vector< EMTG_outerloop_solution >& get_population();
			const std::vector< EMTG_outerloop_solution >& get_archive();
			void set_CR(double const & CR_in);
			void set_mutationrate(double const & mu_in);
			void set_tournament_size(unsigned int const & tournamentsize_in);
			void set_stall_generations(unsigned int const & stall_generations_in);
			void set_populationsize(unsigned int const & populationsize_in);
			void set_max_generations(unsigned int const & max_generations_in);
			void set_tolfit(std::vector<double> const & tolfit_in);
			void set_current_generation(const int& gen_in);
			void set_objectives(const int& number_of_objectives_in, const std::vector<std::string>& objective_descriptions_in);
	};
} //close namespace

#endif //OUTERLOOP_NSGAII