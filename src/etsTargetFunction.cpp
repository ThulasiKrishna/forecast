#include <cmath>
#include <R.h>

//for isnan, math.h is needed
//#include <math.h>

#include "etsTargetFunction.h"

#include <R_ext/Print.h>

//using namespace std;

EtsTargetFunction* EtsTargetFunction::EtsTargetFunctionSingleton = 0;

void EtsTargetFunction::deleteTargetFunctionSingleton() {
	if( EtsTargetFunctionSingleton == 0 ) return;
	else {
		delete EtsTargetFunctionSingleton;
		EtsTargetFunctionSingleton = 0;
	}
	return;
}

EtsTargetFunction* EtsTargetFunction::getTargetFunctionSingleton() {
	if( EtsTargetFunctionSingleton == 0 )
		EtsTargetFunctionSingleton = new EtsTargetFunction();
	return EtsTargetFunctionSingleton;
}


void EtsTargetFunction::init(std::vector<double> & p_y, int p_nstate, int p_errortype,
		int p_trendtype, int p_seasontype, bool p_damped,
		std::vector<double> & p_lower, std::vector<double> & p_upper, std::string p_opt_crit,
		double p_nmse, std::string p_bounds, int p_m,
		bool p_useAlpha, bool p_useBeta, bool p_useGamma, bool p_usePhi,
		double alpha, double beta, double gamma, double phi) {

	//this->par = p_par;
	this->y = p_y;
	this->n = this->y.size();
	this->nstate = p_nstate;
	this->errortype = p_errortype;

	this->trendtype = p_trendtype;
	this->seasontype = p_seasontype;
	this->damped = p_damped;

	//this->par_noopt = p_par_noopt;
	this->lower = p_lower;
	this->upper = p_upper;

	this->opt_crit = p_opt_crit;
	this->nmse = p_nmse;
	this->bounds = p_bounds;

	this->m = p_m;

	this->useAlpha = p_useAlpha;
	this->useBeta = p_useBeta;
	this->useGamma = p_useGamma;
	this->usePhi = p_usePhi;

	//	Rprintf("useAlpha: %d\n", useAlpha);
	//	Rprintf("useBeta: %d\n", useBeta);
	//	Rprintf("useGamma: %d\n", useGamma);
	//	Rprintf("usePhi: %d\n", usePhi);

	//	int j=0;
	//	if(useAlpha) this->alpha = par[j++];
	//	if(useBeta) this->beta = par[j++];
	//	if(useGamma) this->gamma = par[j++];
	//	if(usePhi) this->phi = par[j++];

	this->alpha = alpha;
	this->beta = beta;
	this->gamma = gamma;
	this->phi = phi;

	this->lik = 0;
	this->objval = 0;
	//	for(int i=0; i < 10; i++) this->amse.push_back(0);
	//	for(int i=0; i < n; i++) this->e.push_back(0);

	this->amse.resize(10, 0);
	this->e.resize(n, 0);

}

void EtsTargetFunction::eval(const double* p_par, int p_par_length) {

	bool equal=true;

	//	---------show params----------
	//	Rprintf("par: ");
	//	for(int j=0;j < p_par_length;j++) {
	//		Rprintf("%f ", p_par[j]);
	//	}
	//	Rprintf(" objval: %f\n", this->objval);
	//Rprintf("\n");
	//	---------show params----------

	// Check if the parameter configuration has changed, if not, just return.
	if(p_par_length != this->par.size()) {
		equal=false;
	} else {
		for(int j=0;j < p_par_length;j++) {
			if(p_par[j] != this->par[j]) {
				equal=false;
				break;
			}
		}
	}

	if(equal) return;

	this->par.clear();

	for(int j=0;j < p_par_length;j++) {
		this->par.push_back(p_par[j]);
	}

	int j=0;
	if(useAlpha) this->alpha = par[j++];
	if(useBeta) this->beta = par[j++];
	if(useGamma) this->gamma = par[j++];
	if(usePhi) this->phi = par[j++];

	if(!this->check_params()) {
		this->objval = 1e12;
		return;
	}

	this->state.clear();

	for(int i=par.size()-nstate; i < par.size(); i++) {

		this->state.push_back(par[i]);
	}

	// Add extra state
	if(seasontype!=0) {//"N"=0, "M"=2
		//init.state <- c(init.state, m*(seasontype==2) - sum(init.state[(2+(trendtype!=0)):nstate]))

		double sum=0;

		for(int i=(1+((trendtype!=0) ? 1 : 0));i<nstate;i++) {
			sum += state[i];
		}

		double new_state = m*((seasontype==2) ? 1 : 0) - sum;

		state.push_back(new_state);
	}

	// Check states
	if(seasontype==2)
	{

		double min = state[0];
		int leaveOut = 1;
		if(trendtype!=0) leaveOut++;

		for(int i=0; i<(state.size()-leaveOut); i++) {
			if(state[i] < min) min = state[i];
		}

		if(min < 0) {
			this->objval = 1e8;
			return;
		}

		//  seas.states <- init.state[-(1:(1+(trendtype!=0)))]
		//if(min(seas.states) < 0)
		//  return(1e8)
	};

	//Rprintf(" 3: %f\n", this->objval);

	int p = state.size();

	for(int i=0; i <= p*this->y.size(); i++) state.push_back(0);

	etscalc(&this->y[0], &this->n, &this->state[0], &this->m, &this->errortype, &this->trendtype, &this->seasontype,
			&this->alpha, &this->beta, &this->gamma, &this->phi, &this->e[0], &this->lik, &this->amse[0]);


	//TODO: I don't really understand what this is for..
	// Avoid perfect fits
	if (this->lik < -1e10) this->lik = -1e10;

	// isnan() is a C99 function
	//if (isnan(this->lik)) this->lik = 1e8;
	if (ISNAN(this->lik)) this->lik = 1e8;

	if(abs(this->lik+99999) < 1e-7) this->lik = 1e8;

	  if(this->opt_crit=="lik") this->objval = this->lik;
	  else if(this->opt_crit=="mse") this->objval = this->amse[0];
	  else if(this->opt_crit=="amse") {

		  //return(mean(e$amse[1:nmse]))
		  double mean=0;
		  for(int i=0;i<nmse;i++) {
			  mean+=amse[i]/nmse;
		  }
		  this->objval=mean;

	  }
	  else if(this->opt_crit=="sigma") {
		  //return(mean(e$e^2))
		  double mean=0;
		  int ne=e.size();
		  for(int i=0;i<ne;i++) {
			  mean+=e[i]*e[i]/ne;
		  }
		  this->objval=mean;

	  }
	  else if(this->opt_crit=="mae") {
		  //return(mean(abs(e$e)))

		  double mean=0;
		  int ne=e.size();
		  for(int i=0;i<ne;i++) {
			  mean+=abs(e[i])/ne;
		  }
		  this->objval=mean;

	  }





	//	Rprintf(" lik: %f\n", this->lik);

    //return(list(lik=Cout[[13]], amse=Cout[[14]], e=e, states=matrix(Cout[[3]], nrow=n+1, ncol=p, byrow=TRUE)))

}



bool EtsTargetFunction::check_params() {

	if(bounds != "admissible")
	{
		if(useAlpha)
		{
			if(alpha < lower[0] || alpha > upper[0])
				return(false);
		}
		if(useBeta)
		{
			if(beta < lower[1] || beta > alpha || beta > upper[1])
				return(false);
		}
		if(usePhi)
		{
			if(phi < lower[3] || phi > upper[3])
				return(false);
		}
		if(useGamma)
		{
			if(gamma < lower[2] || gamma > 1-alpha || gamma > upper[2])
				return(false);
		}
	}
	if(bounds != "usual")
	{
		if(!admissible()) return(false);
	}
	return(TRUE);

}


bool EtsTargetFunction::admissible() {

	if(phi < 0 || phi > 1+1e-8) return(false);

	//TODO: What happens if gamma was set by user?
	if(!useGamma) {
		if(alpha < 1-1/phi || alpha > 1+1/phi) return(false);

		if(useBeta)
		{
			if(beta < alpha * (phi-1) || beta > (1+phi)*(2-alpha)) return(false);
		}
	}

	else if(m > 1) //Seasonal model
	{
		//TODO: What happens if beta was set by user?
		if(!useBeta) beta = 0;


		//max(1-1/phi-alpha,0)
		double d = 1-1/phi-alpha;
		if(gamma < ((d > 0) ? d : 0) || gamma > 1+1/phi-alpha) return(false);

		if(alpha < 1-1/phi-gamma*(1-m+phi+phi*m)/(2*phi*m)) return(false);

		if(beta < -(1-phi)*(gamma/m+alpha)) return(false);

		// End of easy tests. Now use characteristic equation

		std::vector<double> opr;
		opr.push_back(1);
		opr.push_back(alpha+beta-phi);

		for(int i=0;i<m-2;i++) opr.push_back(alpha+beta-alpha*phi);

		opr.push_back(alpha+beta-alpha*phi+gamma-1);
		opr.push_back(phi*(1-alpha-gamma));

		int degree = opr.size()-1;

		std::vector<double> opi;
		opi.resize(opr.size(),0);

		std::vector<double> zeror(degree);
		std::vector<double> zeroi(degree);

		Rboolean fail;

		cpolyroot(&opr[0], &opi[0], &degree, &zeror[0], &zeroi[0], &fail);

		double max = 0;
		for(int i=0;i<zeror.size();i++) {
			if(abs(zeror[i])>max) max = abs(zeror[i]);
		}

		//Rprintf("maxpolyroot: %f\n", max);

		if(max > 1+1e-10) return(false);

		// P <- c(phi*(1-alpha-gamma),alpha+beta-alpha*phi+gamma-1,rep(alpha+beta-alpha*phi,m-2),(alpha+beta-phi),1)
		// roots <- polyroot(P)
		// if(max(abs(roots)) > 1+1e-10) return(false);
	}

	//Passed all tests
	return(true);
}


