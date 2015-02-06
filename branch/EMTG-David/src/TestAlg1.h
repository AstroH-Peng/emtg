#include "problem.h"
#include "mission.h"

#include "boost/random/uniform_int.hpp"
#include "boost/random/uniform_real.hpp"
#include "boost/random/mersenne_twister.hpp"

#ifndef _TESTALG1
#define _TESTALG1

namespace EMTG { namespace Solvers {

	class TestAlg1
	{
	public:

		//default constructor
		TestAlg1(void);

		//constructor with a problem object
		TestAlg1(EMTG::problem* Problem_input);

		//destructor
		virtual ~TestAlg1(void);

		//methods
		void Initialize(EMTG::problem* Problem_input);
		//void GenerateNewPopulation();
		//void EvaluateIndividual(vector<double>& X, double* ObjectiveFunctionValue, double* ConstraintNormValue);
		void Evolve();
		void RandInit(int gens,bool sed);
		void EvalInd(vector<double>& X, double* ObjectiveFunctionValue, double* ConstraintNormValue);
		
		
		std::vector<vector<vector<double>>> Bins;
		std::vector<vector<vector<double>>> Fits;



		//fields
		int NP;
		int nX;
		int nF;

		double conWeight;
		double conLW;
		
		//int AgeLimit;
		//double Tau1; //adaptation parameter from jDE
		//double Tau2; //adaptation parameter from jDE
		//double ScaleFactorLowerBound; //adaptation parameter from jDE
		//double ScaleFactorUpperBound; //adaptation parameter from jDE
		//double ScaleFactor;
		//double CrossoverRatio;

		//vector< vector<double> > CurrentGenerationPopulation;
		//vector< vector<double> > NextGenerationPopulation;
		//vector<double> CurrentGenerationObjectiveValue;
		//vector<double> NextGenerationObjectiveValue;
		//vector<double> CurrentGenerationConstraintNorm;
		//vector<double> NextGenerationConstraintNorm;
		std::vector<double> FVector;
		//vector<double> ConstraintViolationVector;
		//vector<double> TrialVector;
		//vector<int> CurrentGenerationAges;
		//vector<int> NextGenerationAges;
		//double TrialObjectiveValue;
		//double TrialConstrainNorm;

		std::vector<double> BestX;
		
		std::vector<double> ConstraintViolationVector;
		
		double BestObjectiveValue;
		double BestConstraintNorm;
		//int BestIndex;
		

		//pointer to problem object
		EMTG::problem* Problem;
		

		//random number generator
		boost::mt19937 RNG;
		//boost::uniform_int<> IntegerDistributionNP;
		
		boost::uniform_real<> DoubleDistribution;
	private:
		double objValTemp;
		double conValTemp;
		boost::uniform_int<> capDist;
		boost::uniform_int<> binDist;
		EMTG::mission* Mission;
		int pM;
		int mode;
		int genC;
		int gC;
		int gCb;
		std::vector<int> phasePts;
		std::vector<double> zeros;
		std::vector<double> trashFits;
		std::vector<double> xTemp;
		//std::vector<double> hist;
		//double fitThresh;
		int iter;
		double varPer;
		double phi;
		double resphi;
		double killDist;
		bool proxMode;
		bool proxKill;
		double elitePer;
		int recur;
		int recurLim;
		int optLim;
		string mutStr;
		double steepStep;
		std::vector<double> ders;

		void SubmitInd(vector<double> X, double* ObjectiveFunctionValue, double* ConstraintNormValue);
		void SortBin(vector<vector<double>>& bin,vector<vector<double>>& fit,vector<double>& X,vector<double>& score,int cap,int ind);
		void LocalOpt(vector<double>& X,int b);
		void LocOptInd_RandWalk(vector<double>& X,int b);
		void LocOptInd_LinWalk(vector<double>& X);
		void LocOptInd_SteepDescent(vector<double>& X, int b);
		void LocOptInd_SteepDescentv2(vector<double>& X, int b);
		void LocOptInd_ParetoDescent(vector<double>& X, int b);
		void LocOptInd_HybridOpt(vector<double>& X, int b);
		double NormOfDif(vector<double>& A,vector<double>& B);
		std::vector<double> GoldenSearch(vector<double>& A, vector<double>& B, vector<double>& C, double tau, vector<double>& dir, int b);
	};

}}//close namespaces

#endif //_TESTALG1