#include "GTOC7_solution_check.h"


void orbitprop(Body * body, double & delta_t)
{
	double mu_sun = 132712440018.0;
	

	body->M = body->M + sqrt(mu_sun/((body->a)*(body->a)*(body->a)))*delta_t;

	//Correct the final value of M for over 2*pi rotations
	while(body->M>2*PI)
	{
		body->M = body->M-2*PI;
	}
	
	
	//Calculate new E and tru values as well
	body->E = laguerreConway(body->ecc, body->M);
	body->tru = 2.0*atan(sqrt((1.0+body->ecc)/(1.0-body->ecc))*tan(body->E/2.0));

	//Update Cartesian coordinates also
	coe2cartesian(body);
}