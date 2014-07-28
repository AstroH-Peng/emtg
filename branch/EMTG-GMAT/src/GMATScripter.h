/*
* GMATScripter.h
*
* Creation Date: july 1st 2014
* Author: ryne beeson
*
* Purpose: GMATScripter Class, which generates a GMAT script file
*		   following an EMTG run. Methods build on work conducted by Max Schadegg (summer 2013)
*		   and Jacob Englander
*/

#ifndef GMATSCRIPTER_H_
#define GMATSCRIPTER_H_

#include "mission.h"
#include "missionoptions.h"
#include "EMTG_math.h"

#include <fstream> //ofstream
#include <sstream> //stringstream
#include <vector>  
#include <string>


namespace EMTG {

	//declarations
	class gmatscripter;
	class gmatmission;
	class gmatjourney;
	class gmatphase;
	class gmatstep;

//a struct type for gmat finite burn
struct gmat_burn {
	string Name;
	string Type = "FiniteBurn";
	string ThrusterName;
};

//a struct type for gmat tank(s)
struct gmat_tank {
	string Name;
	double FuelMass;
};

//a struct type for gmat thruster(s)
struct gmat_thruster {
	string Name;
	struct gmat_tank Tank;
};

//a struct type for storing and accessing gmat spacecraft information
struct gmat_spacecraft {
	//GMAT Specific Data
	string Name;
	string DateFormat = "TAIModJulian";
	double Epoch;
	double DryMass = 0.0;
	string CoordinateSystem;
	struct gmat_thruster Thruster;
	struct gmat_burn Burn;
	//Auxiliary Data
	bool isForward;
	double flyby_distance_lowerbound;
	double flyby_distance_upperbound;
	double flyby_velocity_lowerbound;
	double flyby_velocity_upperbound;
	double initialconditions[6];
};

//a struct type for forcemodel(s)
struct gmat_forcemodel {
	string Name;
	string CentralBody;
	vector <string> PointMasses;
};

//a struct type for gmat propagator(s) 
struct gmat_propagator {
	string Name;
	struct gmat_forcemodel ForceModel;
	bool isCloseApproach;
};


//Class() 'gmatmission'
class gmatmission {

public:
	//constructors
	gmatmission();
	gmatmission(mission* anemtgmission);

	//destructors
	~gmatmission();

	//members
	mission* emtgmission;
	vector <EMTG::Astrodynamics::body> missionbodies_unique;
	vector <EMTG::Astrodynamics::body> missionbodies;
	bool isLT;    //bool for whether LT   or Impulsive is being used
	bool isFBLT;  //bool for whether FBLT or Sims-Flanagan is being used
	vector <gmatjourney> myjourneys;
	//variables
	vector <vector<string>> variables;

	//method
	void get_mission_bodies() {

		//declaration
		int start_int = 0;

		//a loop structure over journeys and phases to collect all the bodies for the mission 
		for (int j = 0; j < this->emtgmission->number_of_journeys; ++j) {
			if (j > 0) { start_int = 1; }
			//add bodies of each phase for each journey to the missionbodies vector
			for (int p = start_int; p < this->emtgmission->journeys[j].number_of_phases + 1; ++p) {
				//push back body onto the vector
				missionbodies.push_back(this->emtgmission->TheUniverse[j].bodies[this->emtgmission->options.sequence[j][p] - 1]);
			}//end of phase for-statement
		}//end of journeys for-statement

		int body_flag = 0;
		//remove duplicate entries in visited bodies list
		for (int body_index = 0; body_index < missionbodies.size(); ++body_index) {
			body_flag = 0;
			for (int body_index_unique = 0; body_index_unique < missionbodies_unique.size(); ++body_index_unique) {
				if (missionbodies[body_index].spice_ID == missionbodies_unique[body_index_unique].spice_ID) {
					body_flag = 1;
				}
			}
			//if body flag not switch 'on', then the body is unique; add it to the missionbodies_unique vector
			if (body_flag == 0) {
				missionbodies_unique.push_back(missionbodies[body_index]);
			}
		}

		//make sure that each body doesnt start with a number for GMAT's sake
		for (int body_index = 0; body_index < missionbodies.size(); ++body_index) {
			if ((missionbodies[body_index].name[0] == 0) || (missionbodies[body_index].name[0] == 1) || (missionbodies[body_index].name[0] == 2) || (missionbodies[body_index].name[0] == 3) || (missionbodies[body_index].name[0] == 4) || (missionbodies[body_index].name[0] == 5) || (missionbodies[body_index].name[0] == 6) || (missionbodies[body_index].name[0] == 7) || (missionbodies[body_index].name[0] == 8) || (missionbodies[body_index].name[0] == 9)) {
				missionbodies[body_index].name = "A" + missionbodies[body_index].name;
			}
		}
		for (int body_index = 0; body_index < missionbodies_unique.size(); ++body_index) {
			if ((missionbodies_unique[body_index].name[0] == 0) || (missionbodies_unique[body_index].name[0] == 1) || (missionbodies_unique[body_index].name[0] == 2) || (missionbodies_unique[body_index].name[0] == 3) || (missionbodies_unique[body_index].name[0] == 4) || (missionbodies_unique[body_index].name[0] == 5) || (missionbodies_unique[body_index].name[0] == 6) || (missionbodies_unique[body_index].name[0] == 7) || (missionbodies_unique[body_index].name[0] == 8) || (missionbodies_unique[body_index].name[0] == 9)) {
				missionbodies_unique[body_index].name = "A" + missionbodies_unique[body_index].name;
			}
		}
	}//end of get_mission_bodies() method

	//method
	void setMissionThrustType() {
		//what type of mission is being solved
		//TODO: What about mission type 5? and confirm mission type 6-9 results in a mission type of 0-4.
		if (this->emtgmission->options.mission_type == 0 || this->emtgmission->options.mission_type == 1 || this->emtgmission->options.mission_type == 4) {
			isLT = false;
			isFBLT = false;
		}
		else if (this->emtgmission->options.mission_type == 2) {
			isLT = true;
			isFBLT = false;
		}
		else if (this->emtgmission->options.mission_type == 3) {
			isLT = true;
			isFBLT = true;
		}
	}

	//method
	void setVariable(string avariable) {
		//declarations
		vector <string> tempvector;
		tempvector.push_back(avariable);
		variables.push_back(tempvector);
	}

	//method
	void setVariable(string avariable, string anassignment) {
		//declarations
		vector <string> tempvector;
		tempvector.push_back(avariable);
		tempvector.push_back(" = " + anassignment);
		variables.push_back(tempvector);
	}

};


//Class() 'gmatjourney'
class gmatjourney {

public:
	//constructors
	gmatjourney();
	gmatjourney(gmatmission* amission, int journey);

	//destructors
	~gmatjourney();

	//members
	gmatmission* mymission;
	int j;
	string id;
	int number_of_emtg_phases;
	vector <gmatphase> myphases;
	//variables
	vector <vector<string>> variables;

	//method
	void setVariable(string avariable) {
		//declarations
		vector <string> tempvector;
		tempvector.push_back(avariable);
		variables.push_back(tempvector);
	}

	//method
	void setVariable(string avariable, string anassignment) {
		//declarations
		vector <string> tempvector;
		tempvector.push_back(avariable);
		tempvector.push_back(" = " + anassignment);
		variables.push_back(tempvector);
	}

};


//Class() 'gmatphase'
class gmatphase {

public:
	//constructors
	gmatphase();
	gmatphase(gmatjourney* ajourney, int phase) {
		myjourney = ajourney;
		p = phase;
		id = myjourney->id + "p" + std::to_string(p);
		if (p == 0) { isFirstPhase = true; }
		if (p == (p == myjourney->number_of_emtg_phases - 1)) { isLastPhase = true; }
		this->set_names();
		this->set_fuelmass_epoch();
		this->get_my_bodies();
		this->get_flyby_data();
		this->set_initialconditions();
	}

	//destructor
	~gmatphase(){};

	//members
	gmatjourney* myjourney;
	int p;
	string id;
	struct gmat_spacecraft spacecraft_forward;
	struct gmat_spacecraft spacecraft_backward;
	vector <EMTG::Astrodynamics::body> mybodies;
	bool isFirstPhase = false;
	bool isLastPhase = false;
	vector <gmatstep> mysteps;
	//variables
	vector <vector<string>> variables;


	//method
	void set_names() {
		//assign names to my members
		spacecraft_forward.Name = "SpaceCraft_" + this->id + "_Forward";
		spacecraft_forward.Thruster.Name = "Thruster_" + this->id + "_Forward";
		spacecraft_forward.Thruster.Tank.Name = "FuelTank_" + this->id + "_Forward";
		spacecraft_forward.Burn.Name = "FiniteBurn_" + this->id + "_Forward";
		spacecraft_forward.Burn.ThrusterName = spacecraft_forward.Thruster.Name;
		spacecraft_forward.isForward = true;

		spacecraft_backward.Name = "SpaceCraft_" + this->id + "_Backward";
		spacecraft_backward.Thruster.Name = "Thruster_" + this->id + "_Backward";
		spacecraft_backward.Thruster.Tank.Name = "FuelTank_" + this->id + "_Backward";
		spacecraft_backward.Burn.Name = "FiniteBurn_" + this->id + "_Backward";
		spacecraft_backward.Burn.ThrusterName = spacecraft_backward.Thruster.Name;
		spacecraft_backward.isForward = false;
	}

	//method
	void set_fuelmass_epoch() {
		//forward spacecraft
		spacecraft_forward.Thruster.Tank.FuelMass = this->myjourney->mymission->emtgmission->journeys[myjourney->j].phases[p].state_at_beginning_of_phase[6];
		spacecraft_forward.Epoch = (this->myjourney->mymission->emtgmission->journeys[myjourney->j].phases[p].phase_start_epoch / 86400.0) + 2400000.5 - 2430000;
		//backward spacecraft
		spacecraft_backward.Thruster.Tank.FuelMass = this->myjourney->mymission->emtgmission->journeys[myjourney->j].phases[p].state_at_end_of_phase[6];
		spacecraft_backward.Epoch = (this->myjourney->mymission->emtgmission->journeys[myjourney->j].phases[p].phase_end_epoch / 86400.0) + 2400000.5 - 2430000;
	}

	//method
	void get_my_bodies() {
		mybodies.push_back(this->myjourney->mymission->missionbodies[p]);
		mybodies.push_back(this->myjourney->mymission->missionbodies[p + 1]);
	}

	//method
	void get_flyby_data() {
		if (isFirstPhase || isLastPhase) {
			//TODO:: hardcoded in for journey arrivals and departures :-/
			if (mybodies[0].mass < 1.0e25) {
				spacecraft_forward.flyby_distance_lowerbound = -10 * (mybodies[0].minimum_safe_flyby_altitude + mybodies[0].radius);
				spacecraft_forward.flyby_distance_upperbound = 10 * (mybodies[0].minimum_safe_flyby_altitude + mybodies[0].radius);
			}
			else {
				spacecraft_forward.flyby_distance_lowerbound = -300 * (mybodies[0].minimum_safe_flyby_altitude + mybodies[0].radius);
				spacecraft_forward.flyby_distance_upperbound = 300 * (mybodies[0].minimum_safe_flyby_altitude + mybodies[0].radius);
			}
			if (mybodies[1].mass < 1.0e25) {
				spacecraft_backward.flyby_distance_lowerbound = -10 * (mybodies[1].minimum_safe_flyby_altitude + mybodies[1].radius);
				spacecraft_backward.flyby_distance_upperbound = 10 * (mybodies[1].minimum_safe_flyby_altitude + mybodies[1].radius);
			}
			else {
				spacecraft_backward.flyby_distance_lowerbound = -300 * (mybodies[1].minimum_safe_flyby_altitude + mybodies[1].radius);
				spacecraft_backward.flyby_distance_upperbound = 300 * (mybodies[1].minimum_safe_flyby_altitude + mybodies[1].radius);
			}
			spacecraft_forward.flyby_velocity_lowerbound = -sqrt(2 * mybodies[0].mu / (mybodies[0].radius + mybodies[0].minimum_safe_flyby_altitude) + (-25) * (-25));
			spacecraft_forward.flyby_velocity_upperbound = sqrt(2 * mybodies[0].mu / (mybodies[0].radius + mybodies[0].minimum_safe_flyby_altitude) + (-25) * (-25));
			spacecraft_backward.flyby_velocity_lowerbound = -sqrt(2 * mybodies[1].mu / (mybodies[1].radius + mybodies[1].minimum_safe_flyby_altitude) + (-25) * (-25));
			spacecraft_backward.flyby_velocity_upperbound = sqrt(2 * mybodies[1].mu / (mybodies[1].radius + mybodies[1].minimum_safe_flyby_altitude) + (-25) * (-25));
		}
		else {
			for (int iX = 0; iX < this->myjourney->mymission->emtgmission->Xdescriptions.size(); ++iX) {
				//find index in Xdescriptions where the flyby altitude bounds for that phase are located
				if (this->myjourney->mymission->emtgmission->Xdescriptions[iX] == id + ": flyby altitude constraint (above minimum altitude but below [100x/300x] altitude for [rocky/gas] planets") {
					spacecraft_forward.flyby_distance_lowerbound = this->myjourney->mymission->emtgmission->Xlowerbounds[iX] * (mybodies[0].minimum_safe_flyby_altitude + mybodies[0].radius);
					spacecraft_forward.flyby_distance_upperbound = -this->myjourney->mymission->emtgmission->Xupperbounds[iX] * (mybodies[0].minimum_safe_flyby_altitude + mybodies[0].radius);
					spacecraft_backward.flyby_distance_lowerbound = this->myjourney->mymission->emtgmission->Xlowerbounds[iX] * (mybodies[1].minimum_safe_flyby_altitude + mybodies[1].radius);
					spacecraft_backward.flyby_distance_upperbound = -this->myjourney->mymission->emtgmission->Xupperbounds[iX] * (mybodies[1].minimum_safe_flyby_altitude + mybodies[1].radius);
				}
				//find index in Xdescriptions where the flyby velocity bounds for that phase are located
				if (this->myjourney->mymission->emtgmission->Xdescriptions[iX] == id + ": initial velocity increment x") {
					spacecraft_forward.flyby_distance_lowerbound = -sqrt(2 * mybodies[0].mu / (mybodies[0].radius + mybodies[0].minimum_safe_flyby_altitude) + this->myjourney->mymission->emtgmission->Xlowerbounds[iX] * this->myjourney->mymission->emtgmission->Xlowerbounds[iX]);
					spacecraft_forward.flyby_distance_upperbound = sqrt(2 * mybodies[0].mu / (mybodies[0].radius + mybodies[0].minimum_safe_flyby_altitude) + this->myjourney->mymission->emtgmission->Xupperbounds[iX] * this->myjourney->mymission->emtgmission->Xupperbounds[iX]);
					spacecraft_backward.flyby_distance_lowerbound = -sqrt(2 * mybodies[1].mu / (mybodies[1].radius + mybodies[1].minimum_safe_flyby_altitude) + this->myjourney->mymission->emtgmission->Xlowerbounds[iX] * this->myjourney->mymission->emtgmission->Xlowerbounds[iX]);
					spacecraft_backward.flyby_distance_upperbound = sqrt(2 * mybodies[1].mu / (mybodies[1].radius + mybodies[1].minimum_safe_flyby_altitude) + this->myjourney->mymission->emtgmission->Xupperbounds[iX] * this->myjourney->mymission->emtgmission->Xupperbounds[iX]);
				}
			}
		}
	}//end of method

	//method
	void set_initialconditions() {
		//declarations
		double epoch;
		double body_states[6];

		//get and set forward spacecraft initial conditions
		epoch = this->myjourney->mymission->emtgmission->journeys[this->myjourney->j].phases[this->p].phase_start_epoch;
		this->mybodies[0].locate_body(epoch, body_states, false, &this->myjourney->mymission->emtgmission->options);
		for (int i = 0; i < 6; ++i) { 
			spacecraft_forward.initialconditions[i] = this->myjourney->mymission->emtgmission->journeys[this->myjourney->j].phases[this->p].state_at_beginning_of_phase[i] - body_states[i]; 
		}

		//get and set backward spacecraft initial conditions
		epoch = this->myjourney->mymission->emtgmission->journeys[this->myjourney->j].phases[this->p].phase_end_epoch;
		this->mybodies[1].locate_body(epoch, body_states, false, &this->myjourney->mymission->emtgmission->options);
		for (int i = 0; i < 6; ++i) {
			spacecraft_backward.initialconditions[i] = this->myjourney->mymission->emtgmission->journeys[this->myjourney->j].phases[this->p].state_at_end_of_phase[i] - body_states[i];
		}
	}

	//method
	virtual void append_step(gmatstep agmatstep);

	//method
	void setVariable(string avariable) {
		//declarations
		vector <string> tempvector;
		tempvector.push_back(avariable);
		variables.push_back(tempvector);
	}

	//method
	void setVariable(string avariable, string anassignment) {
		//declarations
		vector <string> tempvector;
		tempvector.push_back(avariable);
		tempvector.push_back(" = " + anassignment);
		variables.push_back(tempvector);
	}

};


//Class() 'gmatstep'
class gmatstep {

public:
	//constructors
	gmatstep();
	gmatstep(gmatphase* aphase, int step) {
		myphase = aphase;
		s = step;
		gs = myphase->mysteps.size();
		id = myphase->id + "gs" + std::to_string(gs);
		this->find_my_spacecraft();
		this->set_names();
		this->isMatchPoint();
		this->usePushBackStill();
		this->set_stepsize();
		this->set_epochs();
		this->set_r_v_wrt_to_body();
		this->set_thrust();
		this->set_zeroPropagate();
	};

	//destructor
	~gmatstep(){};

	//members
	gmatphase* myphase;
	struct gmat_spacecraft* myspacecraft;
	int s;     //original emtg 's' step number
	int gs;    //gmat step number
	string id; //gmat step id
	struct gmat_propagator propagator; //the propagator to use for my "gmat step"
	vector <double> thrustvector;
	vector <string> thrustvectornames;
	double stepsize;
	double iepoch;
	double fepoch;
	double initial_position_diff[3];
	double initial_velocity_diff[3];
	double final_position_diff[3];
	double final_velocity_diff[3];
	bool inSOIatStart;
	bool inSOIatEnd;
	bool isMatchPointStep;
	bool usePushBack;
	int theInsertIndex;
	bool zeroPropagate = false;
	//variables
	vector <vector<string>> variables;
	//collection members
	vector <string> vary; //collection of vary 'variables' to be performed in the optimize sequence. (n x 1)
	vector <vector <string>> calculate; //collection of calculate 'variables' to be performed in the optimize sequence. n x (2 x 1)
	vector <vector <string>> constraints; //collection of constraint 'variables' to be performed in the optimize sequence. n x (3 x 1)
	//temporary variables for DEBUGGING
	bool flag0 = false;
	bool flag1 = false;

	//method
	void find_my_spacecraft() {
		if (s < (this->myphase->myjourney->mymission->emtgmission->options.num_timesteps / 2)) {
			myspacecraft = &this->myphase->spacecraft_forward;
		}
		else {
			myspacecraft = &this->myphase->spacecraft_backward;
		}
	}

	//method
	void set_names() {
		//declaration
		vector <string> tempvector;

		//create and store my thrust vector GMAT variable name
		thrustvectornames.push_back("ThrustVector_" + this->id + "_Direction1");
		thrustvectornames.push_back("ThrustVector_" + this->id + "_Direction2");
		thrustvectornames.push_back("ThrustVector_" + this->id + "_Direction3");
		//store my thrust vector name, allowing my to vary during the optimization sequence
		vary.push_back(thrustvectornames[0]);
		vary.push_back(thrustvectornames[1]);
		vary.push_back(thrustvectornames[2]);
		//we will need to ensure that the thrust vector remains a unit vector during optimization, therefore 
		//we introduce a constraint. 
		//first calculate the thrust vector magnitude, then constrain it
		tempvector.push_back("ThrustUnitVectorMagnitude_" + this->id);
		tempvector.push_back("ThrustUnitVectorMagnitude_" + this->id);
		tempvector.push_back("sqrt(( " + thrustvectornames[0] + " * 2 - 1) ^ 2 + ( " + thrustvectornames[1] + " * 2 - 1) ^ 2 + ( " + thrustvectornames[2] + " * 2 - 1) ^ 2 )");
		calculate.push_back(tempvector);
		tempvector.pop_back();
		tempvector.erase(tempvector.begin());
		tempvector.push_back("<=");
		tempvector.push_back("1");
		constraints.push_back(tempvector);
		tempvector.clear();
		//use the 'Equation' command (i.e. "calculate") in GMAT to assign the thrust directions
		tempvector.push_back(myspacecraft->Thruster.Name + ".ThrustDirection1");
		tempvector.push_back(myspacecraft->Thruster.Name + ".ThrustDirection1");
		tempvector.push_back("( " + this->thrustvectornames[0] + " * 2 - 1 ) / ThrustUnitVectorMagnitude_" + this->id);
		calculate.push_back(tempvector);
		tempvector.clear();
		tempvector.push_back(myspacecraft->Thruster.Name + ".ThrustDirection2");
		tempvector.push_back(myspacecraft->Thruster.Name + ".ThrustDirection2");
		tempvector.push_back("( " + this->thrustvectornames[1] + " * 2 - 1 ) / ThrustUnitVectorMagnitude_" + this->id);
		calculate.push_back(tempvector);
		tempvector.clear();
		tempvector.push_back(myspacecraft->Thruster.Name + ".ThrustDirection3");
		tempvector.push_back(myspacecraft->Thruster.Name + ".ThrustDirection3");
		tempvector.push_back("( " + this->thrustvectornames[2] + " * 2 - 1 ) / ThrustUnitVectorMagnitude_" + this->id);
		calculate.push_back(tempvector);
		tempvector.clear();
		//use the 'Equation' command (i.e. "calculate") in GMAT to assign the thrust level
		tempvector.push_back(myspacecraft->Thruster.Name + ".C1");
		tempvector.push_back(myspacecraft->Thruster.Name + ".C1");
		tempvector.push_back("( ThrusterMaxThrust * ThrustUnitVectorMagnitude_" + this->id + " )");
		calculate.push_back(tempvector);
		tempvector.clear();
	}

	//method
	void isMatchPoint() {
		if (s == ((this->myphase->myjourney->mymission->emtgmission->options.num_timesteps / 2) - 1) || s == (this->myphase->myjourney->mymission->emtgmission->options.num_timesteps / 2)) { isMatchPointStep = true; }
		else { isMatchPointStep = false; }
	}

	//method
	void usePushBackStill() {
		//if the first step then use push_back
		if (gs == 0) { usePushBack = true; }
		//if this step is the first to use insert() instead of use push_back()
		else if (this->myphase->mysteps[gs - 1].isMatchPointStep && !this->myphase->mysteps[gs - 1].myspacecraft->isForward) {
			flag1 = true;
			usePushBack = false;
			theInsertIndex = myphase->mysteps.size() - 1;
		}
		//else, continue to use the push_back method
		else { usePushBack = true; }

		//if the switch to using insert() has started, then continue to use insert()
		if (gs > 1) {
			if (!this->myphase->mysteps[gs - 2].usePushBack) {
				flag0 = true;
				usePushBack = false;
				theInsertIndex = this->myphase->mysteps[gs - 2].theInsertIndex;
			}
		}
	}

	//method
	void set_stepsize() {
		//get the current step size, which by definition can be a variable timestep although we always use uniform in practice
		if (s == 0 || s == this->myphase->myjourney->mymission->emtgmission->options.num_timesteps - 1) {
			stepsize = 0.5*this->myphase->myjourney->mymission->emtgmission->journeys[this->myphase->myjourney->j].phases[this->myphase->p].time_step_sizes[s];
		}
		else {
			if (this->myspacecraft->isForward) {
				stepsize = 0.5*this->myphase->myjourney->mymission->emtgmission->journeys[this->myphase->myjourney->j].phases[this->myphase->p].time_step_sizes[s - 1]
					+ 0.5*this->myphase->myjourney->mymission->emtgmission->journeys[this->myphase->myjourney->j].phases[this->myphase->p].time_step_sizes[s];
			}
			else {
				stepsize = 0.5*this->myphase->myjourney->mymission->emtgmission->journeys[this->myphase->myjourney->j].phases[this->myphase->p].time_step_sizes[s]
					+ 0.5*this->myphase->myjourney->mymission->emtgmission->journeys[this->myphase->myjourney->j].phases[this->myphase->p].time_step_sizes[s + 1];
			}
		}
	}

	//method
	void set_stepsize(double astepsize) {
		//reset stepsize
		if (astepsize < 0.0) { stepsize = -astepsize; }
		else { stepsize = astepsize; }
		//set epochs and r,v w.r.t to body
		this->set_epochs();
		this->set_r_v_wrt_to_body();
	}

	//method
	void set_epochs() {
		//the first step
		if (myphase->mysteps.size() == 0)  {
			iepoch = this->myphase->myjourney->mymission->emtgmission->journeys[this->myphase->myjourney->j].phases[this->myphase->p].phase_start_epoch;
			fepoch = iepoch + stepsize;
		}
		else {
			//still using push_back(), so we can use the back() method call
			if (usePushBack) {
				iepoch = this->myphase->mysteps.back().fepoch;
				fepoch = iepoch + stepsize;
			}
			//not using push_back()
			else {
				iepoch = this->myphase->mysteps[theInsertIndex].fepoch;
				fepoch = iepoch + stepsize;
			}
		}
	}

	//method
	void set_r_v_wrt_to_body() {
		//declarations
		double body_istates[6];
		double body_fstates[6];
		//get body's states at 'iepoch' and 'fepoch'
		if (myspacecraft->isForward) {
			myphase->mybodies[0].locate_body(iepoch, body_istates, false, &this->myphase->myjourney->mymission->emtgmission->options);
			myphase->mybodies[0].locate_body(fepoch, body_fstates, false, &this->myphase->myjourney->mymission->emtgmission->options);
		}
		else {
			myphase->mybodies[1].locate_body(iepoch, body_istates, false, &this->myphase->myjourney->mymission->emtgmission->options);
			myphase->mybodies[1].locate_body(fepoch, body_fstates, false, &this->myphase->myjourney->mymission->emtgmission->options);
		}
		//there is an odd case where we could analyze the 'body_istates' of the first body and 'body_fstates' of the
		//second body across the matchpoint, but this appears unnecessary at this time.
		//compute the state relative difference of the spacecraft and body at 'iepoch'
		if (myspacecraft->isForward) {
			if (s == 0) {
				for (int index = 0; index < 3; ++index) {
					initial_position_diff[index] = this->myphase->myjourney->mymission->emtgmission->journeys[this->myphase->myjourney->j].phases[this->myphase->p].state_at_beginning_of_phase[index] - body_istates[index];
					initial_velocity_diff[index] = this->myphase->myjourney->mymission->emtgmission->journeys[this->myphase->myjourney->j].phases[this->myphase->p].state_at_beginning_of_phase[index + 3] - body_istates[index + 3];
				}
			}
			else {
				for (int index = 0; index < 3; ++index) {
					initial_position_diff[index] = this->myphase->myjourney->mymission->emtgmission->journeys[this->myphase->myjourney->j].phases[this->myphase->p].spacecraft_state[s - 1][index] - body_istates[index];
					initial_velocity_diff[index] = this->myphase->myjourney->mymission->emtgmission->journeys[this->myphase->myjourney->j].phases[this->myphase->p].spacecraft_state[s - 1][index + 3] - body_istates[index + 3];
				}
			}
		}
		else {
			for (int index = 0; index < 3; ++index) {
				initial_position_diff[index] = this->myphase->myjourney->mymission->emtgmission->journeys[this->myphase->myjourney->j].phases[this->myphase->p].spacecraft_state[s][index] - body_istates[index];
				initial_velocity_diff[index] = this->myphase->myjourney->mymission->emtgmission->journeys[this->myphase->myjourney->j].phases[this->myphase->p].spacecraft_state[s][index + 3] - body_istates[index + 3];
			}
		}
		//compute the state relative difference of the spacecraft and body at 'fepoch'
		if (myspacecraft->isForward) {
			for (int index = 0; index < 3; ++index) {
				final_position_diff[index] = this->myphase->myjourney->mymission->emtgmission->journeys[this->myphase->myjourney->j].phases[this->myphase->p].spacecraft_state[s][index] - body_fstates[index];
				final_velocity_diff[index] = this->myphase->myjourney->mymission->emtgmission->journeys[this->myphase->myjourney->j].phases[this->myphase->p].spacecraft_state[s][index + 3] - body_fstates[index + 3];
			}
		}
		else {
			if (s == (this->myphase->myjourney->mymission->emtgmission->options.num_timesteps - 1)) {
				for (int index = 0; index < 3; ++index) {
					final_position_diff[index] = this->myphase->myjourney->mymission->emtgmission->journeys[this->myphase->myjourney->j].phases[this->myphase->p].state_at_end_of_phase[index] - body_fstates[index];
					final_velocity_diff[index] = this->myphase->myjourney->mymission->emtgmission->journeys[this->myphase->myjourney->j].phases[this->myphase->p].state_at_end_of_phase[index + 3] - body_fstates[index + 3];
				}
			}
			else {
				for (int index = 0; index < 3; ++index) {
					final_position_diff[index] = this->myphase->myjourney->mymission->emtgmission->journeys[this->myphase->myjourney->j].phases[this->myphase->p].spacecraft_state[s + 1][index] - body_fstates[index];
					final_velocity_diff[index] = this->myphase->myjourney->mymission->emtgmission->journeys[this->myphase->myjourney->j].phases[this->myphase->p].spacecraft_state[s + 1][index + 3] - body_fstates[index + 3];
				}
			}
		}
		//evaluate whether the spacecraft was in the body's SOI at 'iepoch' and 'fepoch'
		if (myspacecraft->isForward) {
			if (math::norm(initial_position_diff, 3) < myphase->mybodies[0].r_SOI) { inSOIatStart = true; }
			else { inSOIatStart = false; }
			if (math::norm(final_position_diff, 3) < myphase->mybodies[0].r_SOI) { inSOIatEnd = true; }
			else { inSOIatEnd = false; }
		}
		else {
			if (math::norm(initial_position_diff, 3) < myphase->mybodies[1].r_SOI) { inSOIatStart = true; }
			else { inSOIatStart = false; }
			if (math::norm(final_position_diff, 3) < myphase->mybodies[1].r_SOI) { inSOIatEnd = true; }
			else { inSOIatEnd = false; }
		}

	};

	//method
	void set_thrust() {
		//declarations
		int j = myphase->myjourney->j;
		int p = myphase->p;
		double lower_bound = -1.0;
		double upper_bound = 1.0;
		stringstream tempstream;
		//store control history
		for (int index = 0; index < 3; ++index) {
			thrustvector.push_back((this->myphase->myjourney->mymission->emtgmission->journeys[j].phases[p].control[s][index] - lower_bound) / (upper_bound - lower_bound));
			tempstream << thrustvector.back();
			setVariable("ThrustVector_" + this->id + "_Direction" + std::to_string(index + 1), tempstream.str());
			tempstream.str("");
		}
		setVariable("ThrustUnitVectorMagnitude_" + this->id);
	}

	//method
	void setFMandProp(bool amIaCloseApproach) {
		//declarations
		int body_index;
		//we can fill in the FM.PointMasses
		if (myspacecraft->isForward) { body_index = 0; }
		else { body_index = 1; }

		//ForceModel Name
		if (amIaCloseApproach) { propagator.ForceModel.Name = "FM_" + myphase->mybodies[body_index].central_body_name; }
		else { propagator.ForceModel.Name = "FM_" + myphase->mybodies[body_index].central_body_name + "_3rdBodies_" + myphase->mybodies[0].name + "_" + myphase->mybodies[1].name; }
		//CloseApproach Flag
		propagator.isCloseApproach = amIaCloseApproach;
		//Propagator Name
		if (amIaCloseApproach) { propagator.Name = "Propagator_" + propagator.ForceModel.Name + "_CloseApproach"; }
		else { propagator.Name = "Propagator_" + propagator.ForceModel.Name; }
		//based on 'isCloseApproach', my 'p', and whether 'spacecraft.isForward'

		propagator.ForceModel.CentralBody = this->myphase->mybodies[body_index].central_body_name;
		myspacecraft->CoordinateSystem = this->myphase->mybodies[body_index].name + "J2000Eq";
		if (amIaCloseApproach) {
			propagator.ForceModel.PointMasses.push_back(propagator.ForceModel.CentralBody);
		}
		else {
			propagator.ForceModel.PointMasses.push_back(this->myphase->mybodies[0].name);
			propagator.ForceModel.PointMasses.push_back(this->myphase->mybodies[1].name);
		}
	}

	//method
	void set_zeroPropagate() {
		//set the first and the second elements to 'true'. we will turn the second to false if a third element exists.
		if (gs < 2) { zeroPropagate = true; }
		else if (gs >= 2) {
			//turn my zeroPropagate to 'true' and turn the last guy's to 'false'
			zeroPropagate = true;
			for (int index = 0; index < gs; ++index) {
				if (myphase->mysteps[index].gs == gs - 1) { myphase->mysteps[index].zeroPropagate = false; }
			}
		}
	}

	//method
	void reset() {
		//DEBUGGING
		flag0 = false;
		flag1 = false;
		//clear the names. if reusing the 'gmatstep' object but with a new id this is necessary
		thrustvectornames.clear();
		vary.clear();
		calculate.clear();
		constraints.clear();
		variables.clear();
		//update and reset
		gs++;
		id = myphase->id + "gs" + std::to_string(gs);
		this->set_names();
		this->isMatchPoint();
		this->usePushBackStill();
		this->set_stepsize();
		this->set_epochs();
		this->set_r_v_wrt_to_body();
		this->set_thrust();
		this->set_zeroPropagate();
	}

	//method
	//virtual void setVariable(string avariable);
	//virtual void setVariable(string avariable, string anassignment);
	
	//method
	void setVariable(string avariable) {
		//declarations
		vector <string> tempvector;
		tempvector.push_back(avariable);
		variables.push_back(tempvector);
	}

	//method
	void setVariable(string avariable, string anassignment) {
		//declarations
		vector <string> tempvector;
		tempvector.push_back(avariable);
		tempvector.push_back(" = " + anassignment);
		variables.push_back(tempvector);
	}

	//method
	void scale_stepsize(double scalar) {
		stepsize *= scalar;
		this->set_epochs();
		this->set_r_v_wrt_to_body();
	}

};


//Class() 'gmatscripter'
class gmatscripter {

public:
	//constructors
	gmatscripter();
	gmatscripter(mission* mission_in);

	//destructor
	virtual ~gmatscripter();

	//members
	mission*  ptr_gmatmission;
	//missionoptions gmatoptions;
	std::ofstream GMATfile;
	//a temporary file for debugging purposes
	std::ofstream GMATDebug;
	//gmatclass objects
	gmatmission GMATMission;
	//TAIModJOffset = 2400000.5 - 2430000;
	double TAIModJOffset = -29999.5;

	//methods
	//create file
	virtual void create_GMAT_file();
	virtual void create_GMAT_missions();
	virtual void create_GMAT_journeys();
	virtual void create_GMAT_phases();
	virtual void create_GMAT_steps();
	//model setup
	virtual void write_GMAT_preamble();
	virtual void write_GMAT_spacecraft();
	virtual void write_GMAT_hardware();
	virtual void write_GMAT_nonstandardbody();
	virtual void write_GMAT_forcemodels();
	virtual void write_GMAT_propagators();
	virtual void write_GMAT_burns();
	virtual void write_GMAT_coordinatesystems();
	virtual void write_GMAT_solvers();
	virtual void write_GMAT_subscribers();
	virtual void write_GMAT_variables();
	//optimization methods
	virtual void write_GMAT_beginmissionsequence();
	virtual void write_GMAT_initialguess();
	virtual void write_GMAT_optimization();
	//write calls
	virtual void create_GMAT_spacecraft(struct gmat_spacecraft& spacecraft);
	virtual void create_GMAT_fueltank(struct gmat_spacecraft& spacecraft);
	virtual void create_GMAT_thruster(struct gmat_spacecraft& spacecraft);
	virtual void create_GMAT_forcemodel(struct gmat_forcemodel& forcemodel);
	virtual void create_GMAT_propagator(struct gmat_propagator& propagator);
	virtual void create_GMAT_burn(struct gmat_burn& burn);
	virtual void create_GMAT_coordinatesystem(string bodyname);
	virtual void create_GMAT_variables(vector <vector<string>> variablelist);
	virtual void create_GMAT_initialconditions(struct gmat_spacecraft& spacecraft);

	//GMAT Command Methods
	virtual void aux_GMAT_beginburn(string finiteburnobject, string spacecraft_name);
	virtual void aux_GMAT_endburn(string finiteburnobject, string spacecraft_name);
	virtual void aux_GMAT_propagate(string propagatorname, struct gmat_spacecraft* spacecraft);
	virtual void aux_GMAT_propagate(string propagatorname, struct gmat_spacecraft* spacecraft, double elapsed_secs);
	virtual void aux_GMAT_penUp();
	virtual void aux_GMAT_penDown();

	//reports
	virtual void write_GMAT_report(class gmatstep& agmatstep, bool isbeforemaneuver, bool writecontrolhistory);

	//General Purpose Methods
	virtual void aux_GMAT_vary(string object2vary);
	virtual void aux_GMAT_calculate(string gmatmessagename, string object2calculate, string rhs);
	virtual void aux_GMAT_nonlinearconstraint(string object2constrain, string relation, string rhs);

	//writeout the GMAT script
	virtual void write_GMAT_script();

}; // end of class gmatscript

}  // end of EMTG namespace

#endif // end of GMATSCRIPTER_H_

