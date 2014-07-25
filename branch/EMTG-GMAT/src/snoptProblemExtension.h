//* ------- SNOPT PROBLEM EXTENSION
//
//		This header is built to extend a standard snoptProblem (as per snopt v7.x)
//
//		WARNING: This header OVERRIDES some snopt functions.  This means polymorphism for those functions will break
//			To use this properly DO NOT assume polymorphism is functional on snjac or solve().
//			i.e. DO NOT assume a pointer to an original snoptProblem which has been assigned a snoptProblemExtension will
//			call the new child solve().  IT WILL CALL THE PARENT.
//		TO AVOID THIS PROBLEM: only use pointers to snoptProblemExtension or extension objects directly.
//
//
// ---------------------------------------------*/

#include <cstring>
#include <iostream>
#include <cassert>

#include "snoptProblem.hh"
#include "snfilewrapper.hh"
#include "snopt.hh"


#ifndef SNPT_EXT_H
#define SNPT_EXT_H

class snoptProblemExtension : public snoptProblem {

public:
	snoptProblemExtension(bool const & supress_input = false) : snoptProblem() { //extended contructor for enabling Alex's snopt silencer.  Parent default constructor is fine in most cases
#ifdef QUIET_SNOPT
		if (supress_input) {
			initCalled = 1;
			this->setIntParameter((char*) "Print No", 1);
			initCalled = 0;
		};
#endif		
		lencu = 0;
		leniu = 0;
		lenru = 0;

	};

	

	integer getInform() {return inform;};
	integer getNeA() { return neA;};
	integer getNeG() {return neG;};


	void setSummaryFile(char asummaryname[] ) {
		  assert( initCalled = 1 );
		  if (iSumm != 0 ) {
			snclose_( &iSumm );
		  }
		  iSumm = 6;
		  strcpy( summaryname, asummaryname );  prnt_len = strlen(summaryname);
		  snopenappend_( &iSumm, summaryname,   &inform, prnt_len );
		  this->setIntParameter((char*)"Summary file", iSumm);
		}
	
	void setUserMem( integer lencu_in, char * cu_in, integer leniu_in, integer * iu_in, integer lenru_in, doublereal * ru_in) {

		lencu = lencu_in;
		leniu = leniu_in;
		lenru = lenru_in;

		cu = cu_in;
		iu = iu_in;
		ru = ru_in;

	}

	//this is a very similar, but overridden version of computeJac from the parent.  This is NOT polymorphism safe.
	//this method actually passes in the userspace correctly.
	void computeJac() {
		//Ensures all user data has been initialized.
		userDataSet();
		this->snmema( mincw, miniw, minrw );
		if ( mincw > lencw | miniw > leniw | minrw > lenrw ) {
		
			this->realloc( mincw, miniw, minrw );
			
			this->setIntParameter((char*)"Total real workspace   ", lenrw );
			this->setIntParameter((char*)"Total integer workspace", leniw);
		 }
		snjac_( &inform, &neF, &n, usrfun, iAfun, jAvar, &lenA, &neA, A, iGfun, jGvar, &lenG, &neG, x, xlow, xupp, &mincw, &miniw, &minrw, cu, &lencu, iu, &leniu, ru, &lenru, cw, &lencw, iw, &leniw, rw, &lenrw, 8*500, 8*500 );
		  
		fortranStyleAG = 1;

	}
	//this is a very similar, but overridden version of solve from the parent.  This is NOT polymorphism safe.
	//this method actually passes in the userspace correctly.
	void solve( integer starttype ) {
	  assert( initCalled == 1 );
	  
	  userDataSet();
	  
	  if ( neA == -1 | neG == -1 ) {
		std::cout << "Warning: neA and neG must be set before calling" << "snoptProblem::solve()\n";
		exit(1);
	  }
	  integer npname = strlen(Prob);
	  integer nS, nInf;
	  doublereal sInf;
	  this->increment(); //Convert array entries to Fortran style
	  this->setMemory();
	  snopta_( &starttype, &neF, &n, &nxnames, &nFnames, &ObjAdd, &ObjRow, Prob, usrfun, iAfun, jAvar, &lenA, &neA, A, iGfun, jGvar, &lenG, &neG, xlow, xupp, xnames, Flow, Fupp, Fnames, x, xstate, xmul, F, Fstate, Fmul, &inform, &mincw, &miniw, &minrw, &nS, &nInf, &sInf, cu, &lencu, iu, &leniu, ru, &lenru, cw, &lencw, iw, &leniw, rw, &lenrw, npname, 8*(nxnames), 8*(nFnames), 8*500, 8*500);
	  this->decrement();  //Convert array entries to C style
	}

protected:
	integer    lenru, leniu, lencu;
	doublereal *ru;
	integer    *iu;
	char       *cu;
	char summaryname[200];


};




#endif