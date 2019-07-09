#ifndef GOW16_HPP
#define GOW16_HPP
//======================================================================================
// Athena++ astrophysical MHD code
// Copyright (C) 2014 James M. Stone  <jmstone@princeton.edu>
// See LICENSE file for full public license information.
//======================================================================================
//! \file gow16.hpp
//  \brief definitions for chemical network in Gong, Osriker and Wolfire 2016
//======================================================================================

//c++ headers
#include <string> //std::string

// Athena++ classes headers
#include "network.hpp"
#include "../../athena.hpp"
#include "../../athena_arrays.hpp"

//! \class ChemNetwork
//  \brief Chemical Network that defines the reaction rates between species.
class ChemNetwork : public NetworkWrapper {
	friend class RadIntegrator;
	friend class MeshBlock;
  friend class BoundaryValues;
public:
  ChemNetwork(Species *pspec, ParameterInput *pin);
  ~ChemNetwork();

	//a list of species name, used in output
	static const std::string species_names[NSPECIES];


	//Set the rates of chemical reactions, eg. through density and radiation field.
  //k, j, i are the corresponding index of the grid
  void InitializeNextStep(const int k, const int j, const int i);
  //output properties of network. Can be used in eg. ProblemGenerator.
  void OutputProperties(FILE *pf) const;

  //RHS: right-hand-side of ODE. dy/dt = ydot(t, y). Here y are the abundance
  //of species. details see CVODE package documentation.
  void RHS(const Real t, const Real y[NSPECIES], Real ydot[NSPECIES]);
  //Jacobian: Jacobian of ODE. CVODE can also numerically calculate Jacobian if
  //this is not specified. Details see CVODE package documentation.
  void Jacobian(const Real t,
               const Real y[NSPECIES], const Real fy[NSPECIES], 
               Real jac[NSPECIES][NSPECIES],
               Real tmp1[NSPECIES], Real tmp2[NSPECIES], Real tmp3[NSPECIES]);

private:
  Species *pmy_spec_;
	MeshBlock *pmy_mb_;

	//constants
	static const int ngs_ = 6;
	static const int n_cr_ = 7;
	static const int n_2body_ = 31;
	static const int n_ph_ = 6;
	static const int n_gr_ = 5;
	static const int nE_ = 15;
	static const int n_freq_ = n_ph_ + 2;
	static const int index_gpe_ = n_ph_;
	static const int index_cr_ = n_ph_ + 1;
	//other variables
	static const std::string ghost_species_names_[ngs_];
	std::string species_names_all_[NSPECIES+ngs_];//all species
	Real nH_; //density, updated at InitializeNextStep
	//units of density and radiation
	Real unit_density_in_nH_;
	Real unit_length_in_cm_;
	Real unit_vel_in_cms_;
	Real unit_radiation_in_draine1987_;
	Real temperature_;
	Real temp_max_heat_; 
	Real temp_min_cool_; 
	Real temp_min_rates_; 
	Real temp_max_rates_; 
  int is_H2_rovib_cooling_;//whether to include H2 rovibrational cooling
	//maximum temperature above which heating and cooling is turned off 
	int is_const_temp_; //flag for constant temperature
  //CR shielding
  int is_cr_shielding_;
	//parameters of the netowork
	Real zdg_;
	Real xHe_;
	Real xC_std_;
	Real xO_std_;
	Real xSi_std_;
	Real xC_;
	Real xO_;
	Real xSi_;
	Real cr_rate0_;
	//index of species
	static const int iHeplus_;
	static const int iOHx_;
	static const int iCHx_;
	static const int iCO_;
	static const int iCplus_;
	static const int iHCOplus_;
	static const int iH2_;
	static const int iHplus_;
	static const int iH3plus_;
	static const int iH2plus_;
	static const int iOplus_;
	static const int iSiplus_;
	static const int iE_;
	//index of ghost species
	static const int igSi_;
	static const int igC_;
	static const int igO_;
	static const int igHe_;
	static const int ige_;
	static const int igH_;
  //species needed for calculating shielding
  static const int n_cols_ = 4;
  static const int iNHtot_ = 0;
  static const int iNH2_ = 1;
  static const int iNCO_ = 2;
  static const int iNC_ = 3;
	//-------------------chemical network---------------------
	//number of different reactions
	//cosmic ray reactions
	static const int icr_H2_;
	static const int icr_He_;
	static const int icr_H_;
	static const int incr_[n_cr_]; //reactant
	static const int outcr_[n_cr_]; //product
	static const Real kcr_base_[n_cr_]; //coefficents of rates relative to H
	Real kcr_[n_cr_]; //rates for cosmic-ray reactions.
	//2body reactions
	static const int i2body_H2_H;
	static const int i2body_H2_H2;
	static const int i2body_H_e;
	static const int in2body1_[n_2body_];
	static const int in2body2_[n_2body_];
	static const int out2body1_[n_2body_];
	static const int out2body2_[n_2body_];
	static const Real k2Texp_[n_2body_]; //exponent of temp dependance
	static const Real k2body_base_[n_2body_]; //base rate coefficents
	Real k2body_[n_2body_]; //rates for 2 body reacrtions.
  static const Real A_kCHx_;
  static const Real n_kCHx_;
  static const Real c_kCHx_[4];
  static const Real Ti_kCHx_[4];
	//(15) H2 + *H -> 3 *H  and (16) H2 + H2 -> H2 + 2 *H
	//temperature above which collisional dissociation (15), (16) and (17)
	// will be importatant k>~1e-30
	static const Real temp_coll_;
	//photo reactions
	//radiation related variables, used in RadIntegrator
	//radiation field in unit of Draine 1987 field (G0), updated at InitializeNextStep 
	//vector: [Gph, GPE]
	Real rad_[n_freq_];
	Real kph_[n_ph_]; //rates for photo- reactions.
	static const int iph_C_;
	static const int iph_CHx_;
	static const int iph_CO_;
	static const int iph_OHx_;
	static const int iph_H2_;
	static const int iph_Si_;
	static const int inph_[n_ph_];
	static const int outph1_[n_ph_];
	static const Real kph_base_[n_ph_]; //base rate of photo reaction
	static const Real kph_avfac_[n_ph_];//exponent factor in front of AV
	//grain assisted reactions
	static const int igr_H_;
	static const int ingr_[n_gr_];
	static const int outgr_[n_gr_];
	Real kgr_[n_gr_]; //rates for grain assisted reactions.
	//constants for H+, C+, He+ grain recombination.
	// See Draine ISM book table 14.9 page 158, or Weingartner+Draine2001.
	static const Real cHp_[7]; 
	static const Real cCp_[7]; 
	static const Real cHep_[7]; 
	static const Real cSip_[7]; 
	//factor to calculate psi in H+ recombination on grain
	Real psi_gr_fac_;
	//--------------heating and cooling--------
	Real LCR_;
	Real LPE_;
	Real LH2gr_;
	Real LH2pump_;
	Real LH2diss_;
	Real GCII_;
	Real GCI_;
	Real GOI_;
	Real GLya_;
	Real GCOR_;
	Real GH2_;
	Real GDust_;
	Real GRec_;
	Real GH2diss_;
	Real GHIion_;
	//parameters related to CO cooling
	int isNCOeff_LVG_; 
	//these are needed for LVG approximation
	Real gradv_; //abosolute value of velocity gradient in cgs, >0
	Real Leff_CO_max_; //maximum effective length for CO cooling
	//these are needed for assigned NCO and bCO
	Real NCO_;
	Real bCO_;
	//a small number to avoid divide by zero.
	static const Real small_;
	
	//functions
	void UpdateRates(const Real y[NSPECIES+ngs_]);
	void GetGhostSpecies(const Real *y, Real yall[NSPECIES+ngs_]); 
	Real CII_rec_rate_(const Real temp);
	Real dEdt_(const Real y[NSPECIES+ngs_]);
	void OutputRates(FILE *pf) const;
  Real GetStddev(Real arr[], const int len);
  void SetbCO(const int k, const int j, const int i); //set bCO_ for CO cooling
  //set gradients of v and nH for CO cooling
  void SetGrad_v(const int k, const int j, const int i); 
};

#endif // GOW16_HPP