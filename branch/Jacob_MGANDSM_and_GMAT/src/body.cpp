//EMTG body class

#include <string>
#include <vector>
#include <cmath>
#include <fstream>

#include "missionoptions.h"
#include "body.h"
#include "EMTG_math.h"
#include "Astrodynamics.h"
#include "kepler_lagrange_laguerre_conway.h"

#include "SpiceUsr.h"



namespace EMTG {namespace Astrodynamics {

	//default constructor doesn't do anything
	body::body(){}

	//constructor takes in data and calls load_body_data()
	body::body(const int& ibody_code, const string& iname, const string& ishortname,  const int& ispice_ID, const double& imininum_altitude, const double& imass, const double& iradius, const double& iepoch, vector<double>& ireference_angles, vector<double>& iclassical_orbit_elements, const double& iuniverse_mu, const int& icentral_body_SPICE_ID, const string& icentral_body_name, const double& icentral_body_radius, missionoptions* options)
	{
		load_body_data(ibody_code, iname, ishortname, ispice_ID, imininum_altitude, imass, iradius, iepoch, ireference_angles, iclassical_orbit_elements, iuniverse_mu, icentral_body_SPICE_ID, icentral_body_name, icentral_body_radius, options);
	}

	//destructor
	body::~body(){}

	//function to load new data into the body
	void body::load_body_data(const int& ibody_code, const string& iname, const string& ishortname, const int& ispice_ID, const double& imininum_altitude, const double& imass, const double& iradius, const double& iepoch, vector<double>& ireference_angles, vector<double>& iclassical_orbit_elements, const double& iuniverse_mu, const int& icentral_body_SPICE_ID, const string& icentral_body_name, const double& icentral_body_radius, missionoptions* options)
	{
		//copy information from the inputs into the body
		this->name = iname;
		this->short_name = ishortname;
		this->universe_mu = iuniverse_mu;
		this->body_code = ibody_code;
		this->central_body_spice_ID = icentral_body_SPICE_ID;
		this->central_body_name = icentral_body_name;
		this->central_body_radius = icentral_body_radius;

		this->spice_ID = ispice_ID;
		this->minimum_safe_flyby_altitude = imininum_altitude;
		this->mass = imass;
		this->radius = iradius;
		this->reference_epoch = iepoch;
		this->SMA = iclassical_orbit_elements[0];
		this->ECC = iclassical_orbit_elements[1];
		this->INC = iclassical_orbit_elements[2] * EMTG::math::PI / 180.0;
		this->RAAN = iclassical_orbit_elements[3] * EMTG::math::PI / 180.0;
		this->AOP = iclassical_orbit_elements[4] * EMTG::math::PI / 180.0;
		this->MA = iclassical_orbit_elements[5] * EMTG::math::PI / 180.0;
		

		//determine which ephemeris to draw from
		if (options->ephemeris_source == 0)
		{
			this->body_ephemeris_source = 0; //use static ephemeris
			this->ephemeris_start_date = -0;
			this->ephemeris_end_date = 1e+10;
		}
		else if (options->ephemeris_source == 1)
		{
			//first, check to see if the body exists in the currently loaded SPICE kernels
			double temp_state[6];
			double LT_dump;
			spkez_c (spice_ID, reference_epoch - (51544.5 * 86400.0), "J2000", "NONE", central_body_spice_ID, temp_state, &LT_dump);
			if (failed_c())
			{
				cout << "Warning, body " << this->name << " is not defined in the kernel pool at reference epoch " << this->reference_epoch << endl;
				reset_c();
			}


			if (fabs(temp_state[0]) > 1.0e-6 && fabs(temp_state[0]) < 1.0e+50)
			{
                //activate SPICE for this body
				this->body_ephemeris_source = 1; //body can be located using SPICE
                
                //replace the orbit elements with something drawn from SPICE
                double temp_elements[6];
                Astrodynamics::inertial2COE(temp_state, this->universe_mu, temp_elements);
                this->SMA = temp_elements[0];
                this->ECC = temp_elements[1];
                this->INC = temp_elements[2];
                this->RAAN = temp_elements[3];
                this->AOP = temp_elements[4];
                this->MA = temp_elements[5];
			}
			else
			{
				cout << "Warning, body " << this->name << " does not have a SPICE ephemeris file." << endl;
				this->body_ephemeris_source = 0; //use static ephemeris
				this->ephemeris_start_date = 0;
				this->ephemeris_end_date = 1e+10;
			}
		}

		this->J2000_body_equatorial_frame.initialize(ireference_angles[0], ireference_angles[1], ireference_angles[2], ireference_angles[3], ireference_angles[4], ireference_angles[5]);

        //compute additional values
        this->mu = options->G * mass;
        if (this->ECC < 0.2)
            this->r_SOI = this->SMA * pow(this->mu / this->universe_mu, 0.4);
        else
            this->r_SOI = this->SMA * (1 - this->ECC) * pow(this->mu / (3.0 * this->universe_mu), 0.333333333333333333333333);
	}

	//function to find the body state vector at epoch
	int body::locate_body(const double& epoch, double* state, const bool& need_deriv, missionoptions* options) const
	{
		double DT, n, M, E, V[6];

		switch (body_ephemeris_source)
		{
			case 1: //SPICE
				double LT_dump;
				spkez_c(this->spice_ID, epoch - (51544.5 * 86400.0), "J2000", "NONE", this->central_body_spice_ID, state, &LT_dump);

				if (need_deriv)
				{
					double statepert[6];
					spkez_c(this->spice_ID, epoch - (51544.5 * 86400.0) + 10.0, "J2000", "NONE", this->central_body_spice_ID, statepert, &LT_dump);
                    state[6] = (statepert[0] - state[0]) / (10.0);
                    state[7] = (statepert[1] - state[1]) / (10.0);
                    state[8] = (statepert[2] - state[2]) / (10.0);
					state[9] = (statepert[3] - state[3]) / (10.0);
					state[10] = (statepert[4] - state[4]) / (10.0);
					state[11] = (statepert[5] - state[5]) / (10.0);
				}

				break;
			case 0: //static ephemeris
					//TODO static ephemeris is not ready!
					//note, always should give in Earth equatorial J2000 coordinates for internal processing

					DT = ( epoch - this->reference_epoch );
					

					if (this->SMA > 0.0)
						n = sqrt(this->universe_mu / (this->SMA*this->SMA*this->SMA));
					else
						n = sqrt(this->universe_mu / (-this->SMA*this->SMA*this->SMA));
					
					M = this->MA + n*DT;
					M = fmod(M, 2 * EMTG::math::PI);

					E = Kepler::KeplerLaguerreConway(this->ECC, M);
					V[0] = this->SMA; 
					V[1] = this->ECC;
					V[2] = this->INC;
					V[3] = this->RAAN;
					V[4] = this->AOP;

                    V[5] = 2.0*atan(sqrt((1.0 + this->ECC) / (1.0 - this->ECC))*tan(E / 2.0));


					COE2inertial(V, this->universe_mu, state);

					if (need_deriv)
					{
						double r = sqrt(state[0]*state[0] + state[1]*state[1] + state[2]*state[2]);
						double r3 = r*r*r;
						state[6] = -universe_mu/r3 * state[0];
						state[7] = -universe_mu/r3 * state[1];
						state[8] = -universe_mu/r3 * state[2];
					}
				break;
			default:
				cout << "Invalid ephemeris source " << body_ephemeris_source << " for object " << name << endl;
				cout << "Program halted. Press enter to quit." << endl;
#ifndef BACKGROUND_MODE
				cin.ignore();
#endif
		}

		return 0;
	}
	
	//function to locate a point on the sphere of influence in cartesian coordinates (Earth Equatorial J2000, measured from central body of current universe)
    int body::locate_point_on_SOI(const double& theta, 
                                    const double& phi, 
                                    double* point_relative_to_body) const
	{
		point_relative_to_body[0] = this->r_SOI * cos(theta)*cos(phi);
		point_relative_to_body[1] = this->r_SOI * sin(theta)*cos(phi);
		point_relative_to_body[2] = this->r_SOI * sin(phi);

		return 0;
	}

	//function to print body to screen (for debug purposes)
    void body::print_body_to_screen(string filename) const
	{
		ofstream outputfile(filename.c_str(), ios::app);
		outputfile << "Body name: " << this->name << endl;
		outputfile << "Short name: " << this->short_name << endl;
		outputfile << "Body position in menu: " << this->body_code << endl;
		outputfile << "SPICE ID: " << this->spice_ID << endl;
		outputfile << "Valid flyby target? " << (this->minimum_safe_flyby_altitude > 0.0 ? "True" : "False") << endl;
		if (this->minimum_safe_flyby_altitude > 0.0)
			outputfile << "Minimum safe flyby altitude (km) " << this->minimum_safe_flyby_altitude << endl;
		outputfile << "Mass (kg): " << this->mass << endl;
		outputfile << "Radius (km): " << this->radius << endl;
		outputfile << "Ephemeris source: " << this->body_ephemeris_source << endl;
		outputfile << "R_SOI: " << this->r_SOI << endl;
		outputfile << "Reference Epoch (MJD): " << this->reference_epoch << endl;
		outputfile << "SMA (km): " << this->SMA << endl;
		outputfile << "ECC: " << this->ECC << endl;
		outputfile << "INC (deg): " << this->INC * 180.0 / EMTG::math::PI << endl;
		outputfile << "RAAN (deg): " << this->RAAN * 180.0 / EMTG::math::PI << endl;
		outputfile << "AOP (deg): " << this->AOP * 180.0 / EMTG::math::PI << endl;
		outputfile << "MA (deg): " << this->MA * 180.0 / EMTG::math::PI << endl;
		outputfile << endl;

		outputfile.close();
	}
	
	//comparator
    bool body::operator== (const body& OtherBody) const
	{
		//compare three fields for accuracy
		if (this->name == OtherBody.name && this->spice_ID == OtherBody.spice_ID && this->mass == OtherBody.mass)
		{
			return true;
		}
		return false;
	}

    bool body::operator!= (const body& OtherBody) const
	{
		return !(*this == OtherBody);
	}
	
}}//close namespace