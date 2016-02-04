/*
 * CCOperators.h
 *
 *  Created on: Jul 6, 2015
 *      Author: kottmanj
 */

#ifndef CCOPERATORS_H_
#define CCOPERATORS_H_

// Operators for coupled cluster and CIS

#include <chem/CCStructures.h>
#include <chem/projector.h>
#include <chem/nemo.h>
//#include <string>o

// to debug
//#include<chem/mp2.h>

namespace madness {

  template<size_t NDIM>
  static double rsquare(const Vector<double,NDIM> &r){
    double result = 0.0;
    for(size_t i=0;i<r.size();i++){
      result += r[i]*r[i];
    }
    return result;
  }
  template<size_t NDIM>
  static double gauss_ND(const Vector<double,NDIM> &r){
    const double r2=rsquare<NDIM>(r);
    const double double_N = (double) NDIM;
    const double c = pow(1.0/(sqrt(2.0*M_PI)),double_N);
    return c*exp(-0.5*r2);
  }
  template<size_t NDIM>
  static double unitfunction(const Vector<double,NDIM>&r){
    return 1.0;
  }

  // functors for gauss function
  static double f_gauss(const coord_3d &r) {
    return exp(-((r[0]) * (r[0]) + (r[1]) * (r[1]) + (r[2]) * (r[2])));
  }
  static double f_r2(const coord_3d &r) {
    return (r[0] * r[0] + r[1] * r[1] + r[2] * r[2]);
  }
  static double f_r(const coord_3d &r){
    return sqrt(f_r2(r));
  }
  static double f_laplace_gauss(const coord_3d&r) {
    return -6.0 * f_gauss(r) + 4.0 * f_r2(r) * f_gauss(r);
  }

  typedef std::vector<Function<double, 3> > vecfuncT;

  /// Structure that holds the CC intermediates and is able to refresh them
  struct CC_Intermediates {
  public:
    CC_Intermediates(World&world, const CC_vecfunction &bra,
		     const CC_vecfunction &ket, const Nemo&nemo,
		     const CC_Parameters &param) :
		       world(world),
		       parameters(param),
		       mo_bra_(bra),
		       mo_ket_(ket),
		       poisson(std::shared_ptr < real_convolution_3d> (CoulombOperatorPtr(world, parameters.lo,parameters.thresh_poisson))),
		       f12op(std::shared_ptr < real_convolution_3d> (SlaterF12OperatorPtr(world, parameters.gamma(),parameters.lo, parameters.thresh_poisson))),
		       density_(make_density(bra, ket)),
		       exchange_intermediate_(make_exchange_intermediate(bra, ket)),
		       f12_exchange_intermediate_(make_f12_exchange_intermediate(bra, ket)),
		       hartree_potential_(make_hartree_potential(density_)) { }

    // return the whole intermediate (to estimate size)
    const intermediateT& get_EX()const{return exchange_intermediate_;}
    const intermediateT& get_pEX()const{return perturbed_exchange_intermediate_;}
    const intermediateT& get_fEX()const{return f12_exchange_intermediate_;}
    const intermediateT& get_pfEX()const{return perturbed_f12_exchange_intermediate_;}
    const intermediateT& get_rEX()const{return response_exchange_intermediate_;}
    const intermediateT& get_rfEX()const{return response_f12_exchange_intermediate_;}

    /// Get the intermediates
    const real_function_3d get_density() const {
      return density_;
    }
    const real_function_3d get_perturbed_density() const {
      return perturbed_density_;
    }
    const real_function_3d get_hartree_potential() const {
      return hartree_potential_;
    }
    const real_function_3d get_perturbed_hartree_potential() const {
      return perturbed_hartree_potential_;
    }
    /// returns <k|g|l>
    const real_function_3d get_EX(const size_t &k, const size_t &l) const {
      return exchange_intermediate_(k, l);
    }
    const real_function_3d get_EX(const CC_function &k,
				  const CC_function &l) const {
      return exchange_intermediate_(k.i, l.i);
    }
    const real_function_3d get_fEX(const size_t &k, const size_t &l) const {
      return f12_exchange_intermediate_(k, l);
    }
    const real_function_3d get_fEX(const CC_function &k,
				   const CC_function &l) const {
      return f12_exchange_intermediate_(k.i, l.i);
    }

    /// returns <k|g|\tau_l>
    const real_function_3d get_pEX(const CC_function &k,
				   const CC_function &l) const {
      return perturbed_exchange_intermediate_(k.i, l.i);
    }
    const real_function_3d get_pEX(const size_t &k, const size_t &l) const {
      return perturbed_exchange_intermediate_(k, l);
    }
    /// returns <k|f|\tau_l>
    const real_function_3d get_pfEX(const CC_function &k,
				    const CC_function &l) const {
      return perturbed_f12_exchange_intermediate_(k.i, l.i);
    }
    const real_function_3d get_pfEX(const size_t &k, const size_t &l) const {
      return perturbed_f12_exchange_intermediate_(k, l);
    }
    const real_function_3d get_rEX(const CC_function &k,
				    const CC_function &l) const {
      return response_exchange_intermediate_(k.i, l.i);
    }
    const real_function_3d get_rEX(const size_t &k, const size_t &l) const {
      return response_exchange_intermediate_(k, l);
    }
    const real_function_3d get_rfEX(const CC_function &k,
				    const CC_function &l) const {
      return response_f12_exchange_intermediate_(k.i, l.i);
    }
    const real_function_3d get_rfEX(const size_t &k, const size_t &l) const {
      return response_f12_exchange_intermediate_(k, l);
    }

    /// refresh the intermediates that depend on the \tau functions
    void update(const CC_vecfunction &tau) {
      if (world.rank() == 0)
	std::cout << "Update Intermediates:\n";
      perturbed_density_ = make_density(mo_bra_, tau);
      perturbed_hartree_potential_ = (*poisson)(perturbed_density_);
      perturbed_exchange_intermediate_ = make_exchange_intermediate(mo_bra_,tau);
      perturbed_f12_exchange_intermediate_ = make_f12_exchange_intermediate(mo_bra_, tau);
    }

    void update_response(const CC_vecfunction &x){
      std::cout << "Update Response Intermediates:\n";
      response_exchange_intermediate_ = make_exchange_intermediate(mo_bra_,x);
      response_f12_exchange_intermediate_ = make_f12_exchange_intermediate(mo_bra_,x);
    }

    /// make a density from two input functions
    /// For closed shell the density has to be scaled with 2 in most cases (this is not done here!)
    /// @param[in] vecfuncT_bra
    /// @param[in] vecfuncT_ket
    /// @param[out] \sum_i bra_i * ket_i
    //real_function_3d make_density(const vecfuncT &bra,const vecfuncT &ket) const;
    real_function_3d make_density(const CC_vecfunction &bra,
				  const CC_vecfunction &ket) const;
    /// Poisson operator
    std::shared_ptr<real_convolution_3d> get_poisson() const {
      return poisson;
    }

  private:
    World &world;
    const CC_Parameters &parameters;
    const CC_vecfunction &mo_bra_;
    const CC_vecfunction &mo_ket_;
    const std::shared_ptr<real_convolution_3d> poisson;
    const std::shared_ptr<real_convolution_3d> f12op;
    /// const intermediates
    const real_function_3d density_;
    /// Exchange intermediate: \f$EX(i,j) = <i|g|j>\f$
    intermediateT exchange_intermediate_;
    /// The f12 exchange intermediate \f$fEX(i,j) = <i|f12|j>\f$
    intermediateT f12_exchange_intermediate_;
    /// Hartree_Potential  \f$ = J = \sum_k <k|g|k> = \f$ Poisson(density)
    const real_function_3d hartree_potential_;
    /// intermediates that need to be recalculated before every iteration
    /// Perturbed Density \f$= \sum_k |k><\tau_k| \f$
    real_function_3d perturbed_density_;
    /// Perturbed Hartree Potential PJ \f$ = \sum_k <k|g|\tau_k> = \f$ Poisson(perturbed_density)
    real_function_3d perturbed_hartree_potential_;
    /// Perturbed Exchange Intermediate: \f$ PEX(i,j) = <i|g|\tau_j> \f$
    intermediateT perturbed_exchange_intermediate_;
    /// Perturbed f12-exchange-intermediate: \f$ pfEX(i,j) = <i|f12|tau_j> \f$
    intermediateT perturbed_f12_exchange_intermediate_;
    /// response exchange-intermediate: \f$ rEX(i,j) = <i|g12|x_j> \f$
    intermediateT response_exchange_intermediate_;
    /// response f12-exchange-intermediate: \f$ rEX(i,j) = <i|f12|x_j> \f$
    intermediateT response_f12_exchange_intermediate_;

    void error(const std::string &msg) const {
      std::cout << "\n\n\nERROR IN CC_INTERMEDIATES:\n" << msg << "\n\n\n!!!";
      MADNESS_EXCEPTION(
	  "\n\n!!!!ERROR IN CC_INTERMEDIATES!!!!\n\n\n\n\n\n\n\n\n\n\n\n",
	  1);
    }
    void warning(const std::string &msg) const {
      std::cout << "\n\n\nWARNING IN CC_INTERMEDIATES:\n" << msg
	  << "\n\n\n!!!";
    }
  public:
    /// Make the exchange intermediate: EX[j][i] \f$ <bra[i](r2)|1/r12|ket[j](r2)> \f$
    intermediateT make_exchange_intermediate(const CC_vecfunction &bra,
					     const CC_vecfunction &ket) const;
    intermediateT make_f12_exchange_intermediate(const CC_vecfunction &bra,
						 const CC_vecfunction &ket) const;
    /// Calculates the hartree potential Poisson(density)
    /// @param[in] density A 3d function on which the poisson operator is applied (can be the occupied density and the perturbed density)
    /// @return poisson(density) \f$ = \int 1/r12 density(r2) dr2 \f$
    real_function_3d make_hartree_potential(
	const real_function_3d &density) const {
      real_function_3d hartree = (*poisson)(density);
      hartree.truncate();
      return hartree;
    }
  };

  /// Coupled Cluster Operators (all closed shell)
  class CC_Operators {
  public:
    /// Constructor
    CC_Operators(World& world, const Nemo &nemo,
		 const CorrelationFactor &correlationfactor,
		 const CC_Parameters &param) :
		   world(world), nemo(nemo), corrfac(correlationfactor), parameters(
		       param), mo_bra_(make_mo_bra(nemo)), mo_ket_(
			   make_mo_ket(nemo)), orbital_energies(
			       init_orbital_energies(nemo)), intermediates_(world, mo_bra_,
									    mo_ket_, nemo, param), Q12(world) {
      // make operators

      // make the active mo vector (ket nemos, bra is not needed for that)
      MADNESS_ASSERT(mo_ket_.size() == mo_bra_.size());
      // initialize the Q12 projector
      Q12.set_spaces(mo_bra_.get_vecfunction(), mo_ket_.get_vecfunction(),
		     mo_bra_.get_vecfunction(), mo_ket_.get_vecfunction());
      performance_S.current_iteration = 0;
      performance_D.current_iteration = 0;

    }

    // collect all the data for every function and every iteration
    mutable CC_performance performance_S;
    mutable CC_performance performance_D;
    // collect all the warnings that are put out over a calculation
    mutable std::vector<std::string> warnings;

    /// save a function
    template<typename T, size_t NDIM>
    void save_function(const Function<T, NDIM>& f,
		       const std::string name) const;

    void plot(const real_function_3d &f, const std::string &msg) const {
      CC_Timer plot_time(world, "plotting " + msg);
      plot_plane(world, f, msg);
      plot_time.info();
    }

    void error(const std::string &msg) const {
      std::cout << "\n\n\nERROR IN CC_OPERATORS:\n" << msg << "!!!\n\n\n";
      MADNESS_EXCEPTION(
	  "\n\n!!!!ERROR IN CC_OPERATORS!!!!\n\n\n\n\n\n\n\n\n\n\n\n", 1);
    }
    void warning(const std::string &msg, CC_data &data) const {
      warning(msg);
      data.warnings.push_back(msg);
    }
    void warning(const std::string &msg) const {
      if(world.rank()==0){
	std::cout << "\n\n" << std::endl;
	std::cout << "|| || || || || || || || || || || || || " << std::endl;
	std::cout << "\\/ \\/ \\/ \\/ \\/ \\/ \\/ \\/ \\/ \\/ \\/ \\/ \\/" << std::endl;
	std::cout << "=> WARNING IN CC_OPERATORS" << std::endl;
	std::cout << "=> " <<msg << std::endl;
	std::cout << "/\\ /\\ /\\ /\\ /\\ /\\ /\\ /\\ /\\ /\\ /\\ /\\ /\\ " << std::endl;
	std::cout << "|| || || || || || || || || || || || || " << std::endl;
	std::cout << "\n\n" << std::endl;
      }
      warnings.push_back(msg);
      std::ofstream file;
      file.open ("warnings");
      file << msg << "\n";
      file.close();
    }

    void output_section(const std::string&msg) const {
      if (world.rank() == 0) {
	std::cout << "\n\n--------------\n";
	std::cout << msg << std::endl;
	std::cout << "\n";
      }
    }
    void output(const std::string &msg) const {
      if (world.rank() == 0) {
	std::cout << msg << std::endl;
      }
    }

    void update_intermediates(const CC_vecfunction &singles) {
      CC_Timer update(world, "Update Intermediates");
      intermediates_.update(singles);
      update.info();
    }

    CC_function mo_ket(const size_t &i) const {
      return mo_ket_(i);
    }
    CC_vecfunction mo_ket() const {
      return mo_ket_;
    }
    CC_function mo_bra(const size_t &i) const {
      return mo_bra_(i);
    }
    CC_vecfunction mo_bra() const {
      return mo_bra_;
    }

    /// makes the t intermediate which is defined as: \f$ |t_i> = |\tau_i> + |i> \f$
    CC_function make_t_intermediate(const CC_function &tau) const {
      CC_function t(mo_ket_(tau.i).function + tau.function, tau.i, MIXED);
      return t;
    }
    CC_vecfunction make_t_intermediate(const CC_vecfunction &tau) const {
      CC_vecfunction result;
      for (auto x : tau.functions) {
	CC_function tmpi = make_t_intermediate(x.second);
	result.insert(tmpi.i, tmpi);
      }
      return result;
    }

    vecfuncT get_CCS_potential(const CC_vecfunction &singles) const {

      // make a dummy doubles with no content
      Pairs<CC_Pair> doubles;

      vecfuncT result = potential_singles(doubles, singles, pot_F3D_);

      result = add(world, result,
		   potential_singles(doubles, singles, pot_S1_)); // brillouin term
      result = add(world, result,
		   potential_singles(doubles, singles, pot_S5a_)); // brillouin term
      result = add(world, result,
		   potential_singles(doubles, singles, pot_ccs_));
      Q(result);
      truncate(world, result);
      performance_S.current_iteration++;
      return result;
    }

    double make_norm(const CC_function &f)const{return make_norm(f.function);}
    // makes vectorfunction norm = sqrt( \sum_i <fi|fi> )
    double make_norm(const CC_vecfunction &f)const{
      double norm2 = 0.0;
      for(const auto& ktmp:f.functions){
	double tmp = make_norm(ktmp.second);
	norm2+= tmp*tmp;
      }
      return sqrt(norm2);
    }
    double make_norm(const real_function_3d &f)const{
      const real_function_3d bra = f*nemo.nuclear_correlation -> square();
      const double norm2 = bra.inner(f);
      return sqrt(norm2);
    }

    void test_singles_potential();

    vecfuncT get_CC2_singles_potential(const CC_vecfunction &singles,
				       const Pairs<CC_Pair> &doubles) {

      vecfuncT fock_residue = potential_singles(doubles, singles,pot_F3D_);
      vecfuncT result = potential_singles(doubles, singles, pot_ccs_);
      result = add(world, result,potential_singles(doubles, singles, pot_S2b_u_));
      result = add(world, result,potential_singles(doubles, singles, pot_S2c_u_));
      result = add(world, result,potential_singles(doubles, singles, pot_S4a_u_));
      result = add(world, result,potential_singles(doubles, singles, pot_S4b_u_));
      result = add(world, result,potential_singles(doubles, singles, pot_S4c_u_));

      result = add(world, result,potential_singles(doubles, singles, pot_S2b_r_));
      result = add(world, result,potential_singles(doubles, singles, pot_S2c_r_));
      result = add(world, result,potential_singles(doubles, singles, pot_S4a_r_));
      result = add(world, result,potential_singles(doubles, singles, pot_S4b_r_));
      result = add(world, result,potential_singles(doubles, singles, pot_S4c_r_));

      // the fock residue does not get projected, but all the rest
      Q(result);
      truncate(world, result);
      // need to store this for application of Fock oerator on singles ( F|taui> = -Singles_potential[i] + \epsilon_i|taui>)
      current_singles_potential = copy(world,result);
      result = add(world, result, fock_residue);
      performance_S.current_iteration++;
      return result;
    }

    real_function_6d get_CC2_doubles_potential(const CC_Pair &u,const CC_vecfunction &singles) const {
      const real_function_6d coulomb_part = potential_doubles(u, singles, pot_cc2_coulomb_);
      const real_function_6d cc2_residue = potential_doubles(u, singles, pot_cc2_residue_);
      const real_function_6d fock_residue = potential_doubles(u, singles, pot_F6D_);

      real_function_6d potential = coulomb_part + cc2_residue;
      apply_Q12(potential,"coulomb-part+cc2_residue");
      real_function_6d result = fock_residue+potential;
      result.truncate().reduce_rank();
      result.print_size("doubles potential");
      if (world.rank() == 0)performance_D.info(performance_D.current_iteration);
      return result;
    }

    real_function_6d make_cc2_coulomb_parts(const CC_function &taui, const CC_function &tauj, const CC_vecfunction &singles) const;

    // computes: G(f(F-eij)|titj> + Ue|titj> - [K,f]|titj>) and uses G-operator screening
    real_function_6d make_cc2_residue_sepparated(const CC_function &taui, const CC_function &tauj)const;


    // returns \sum_k <k|operator|xy>_1
    real_function_3d screen_operator(const CC_vecfunction &bra,const CC_function &x, const CC_function &y,real_function_3d (*A)(const CC_function&,const CC_function&, const CC_function&))const{
      real_function_3d result = real_factory_3d(world);
      for(const auto & ktmp:bra.functions){
	const CC_function &k=ktmp.second;
	result += A(k,x,y);
      }
      return result;
    }

    // return <k|g|xy>_1 = \sum_k <k|g|x>(2)|y(2)>
    real_function_3d screened_g(const CC_function&k,const CC_function&x, const CC_function&y)const{
      return (apply_g12(k,x)*y.function).truncate();
    }
    // return <k|f|xy>_1 = <k|f|x>(2)|y(2)>
    real_function_3d screened_f(const CC_function&k,const CC_function&x, const CC_function&y)const{
      return (apply_f12(k,x)*y.function).truncate();
    }
    // return <k|F|xy>_1 = <k|F|x> |y(2)> + <k|x> |Fy(2)>
    real_function_3d screened_fF(const CC_function&k,const CC_function&x, const CC_function&y)const{
      // <k|F|x> can be calculated more effectively but since the full Fock operator is applied to y we should keep it consistent
      const real_function_3d Fx = apply_F(x);
      const real_function_3d Fy = apply_F(y);
      const double kFx = k.inner(Fx);
      const double kx =k.inner(x);
      return (kFx*y.function + kx*Fy).truncate();
    }
    // return <k|Kf|xy>_1 = <k|K1f|xy>_1 + <k|K2f|xy>
    // <k|K1f|xy> = <l|kgl*f|x>*|y>
    // <k|K2f|xy> = <l|kfx*g|y>*|l>
    real_function_3d screened_Kf(const CC_function&k,const CC_function&x, const CC_function&y)const{
      const real_function_3d kfx_y = screened_f(k,x,y);
      real_function_3d l_kgl = real_factory_3d(world);
      real_function_3d k_K2f_xy = real_factory_3d(world);
      for(const auto & ltmp:mo_bra_.functions){
	const CC_function &l=ltmp.second;
	l_kgl += l.function*apply_g12(k,mo_ket_(l));
	k_K2f_xy += apply_g12(l,kfx_y)*mo_ket_(l).function;
      }
      l_kgl.truncate();
      k_K2f_xy.truncate();
      const real_function_3d k_K1f_xy = (apply_f12(l_kgl,x)*y.function).truncate();


      return (k_K1f_xy + k_K2f_xy).truncate();

    }

    // return <kl|A|xy>
    // the given operator a should act as: A(x,y,z) = <x|A|yz>_1
    Tensor<double> make_matrix_elements(const CC_function &x, const CC_function &y,real_function_3d (*A)(const CC_function&,const CC_function&, const CC_function&))const{
      Tensor<double> result(mo_bra_.size(),mo_bra_.size());
      for(const auto & ktmp:mo_bra_.functions){
	const CC_function & k=ktmp.second;
	const real_function_3d kfxy = A(k,x,y);
	for(const auto & ltmp:mo_bra_.functions){
	  const CC_function & l=ltmp.second;
	  result(k.i,l.i) = l.inner(kfxy);
	}
      }
      return result;
    }


    real_function_6d get_MP2_potential_constant_part(const CC_Pair &u) const {
      CC_function mo_i = mo_ket_(u.i);
      CC_function mo_j = mo_ket_(u.j);

      CC_Timer timer_U(world, "U|ij>");
      real_function_6d UePart = apply_transformed_Ue(mo_i, mo_j);
      UePart.print_size("Ue|" + mo_i.name() + mo_j.name());
      timer_U.info();

      CC_Timer timer_KffK(world, "Kf|ij>");
      real_function_6d KffKPart = apply_exchange_commutator(mo_i, mo_j);
      KffKPart.print_size("[K,f]|" + mo_i.name() + mo_j.name());
      timer_KffK.info();

      real_function_6d unprojected_result = (UePart - KffKPart).truncate();
      unprojected_result.print_size(
	  "Ue - [K,f]|" + mo_i.name() + mo_j.name());
      real_function_6d result = Q12(unprojected_result);
      result.print_size("Q12(Ue - [K,f]" + u.name());
      return result;

    }

    /// returns the non constant part of the MP2 potential which is
    /// \f$ (2J-K+Un)|uij> \f$
    real_function_6d get_MP2_potential_residue(const CC_Pair &u) const {
      CC_Timer timer(world, "(2J-K(R)+Un)|uij>");
      CC_data data("mp2_residue");
      real_function_6d result = fock_residue_6d(u);
      data.result_size = get_size(result);
      data.result_norm = result.norm2();
      data.time = timer.current_time();
      performance_D.insert(data.name, data);
      timer.info();
      return result;
    }

    /// reconstructs the full pair function from the regularized pair functions
    /// used to compute norms of the doubles to compare with LCAO codes
    /// used to debug the singles potential
    /// @param[in] u the regularized function
    /// @return Equation: \f$ \tau = u + Q12f12(|ij> + |taui,j> + |i,tauj> + |taui,tauj>) = u + Q12f12|titj> \f$ with \f$ ti = taui + i \f$
    real_function_6d make_full_pair_function(const CC_Pair &u,
					     const CC_function &taui, const CC_function &tauj) const {
      const size_t i = u.i;
      const size_t j = u.j;
      MADNESS_ASSERT(i == taui.i);
      MADNESS_ASSERT(j == tauj.i);
      real_function_3d ti = mo_ket_(i).function + taui.function;
      real_function_3d tj = mo_ket_(j).function + tauj.function;
      real_function_6d Q12f12titj = make_f_xy(ti, tj);
      apply_Q12(Q12f12titj);
      real_function_6d result = u.function + Q12f12titj;
      return result;
    }
    CC_Pair make_full_pair(const CC_Pair &u, const CC_function &taui,
			   const CC_function &tauj) const {
      real_function_6d full_pair_function = make_full_pair_function(u, taui,
								    tauj);
      CC_Pair result(full_pair_function, u.i, u.j);
      return result;
    }
    Pairs<CC_Pair> make_full_pairs(const Pairs<CC_Pair> &pairs,
				   const CC_vecfunction &singles) const {
      Pairs<CC_Pair> result;
      for (auto utmp : pairs.allpairs) {
	CC_Pair u = utmp.second;
	CC_Pair ufull = make_full_pair(u, singles(u.i), singles(u.j));
	result.insert(ufull.i, ufull.j, ufull);
      }
      return result;
    }

    Pairs<CC_Pair> make_reg_residues(const CC_vecfunction &singles) const {
      CC_Timer time(world,"Making Regularization-Tails of Pair-Functions");
      Pairs<CC_Pair> result;
      CC_vecfunction t = make_t_intermediate(singles);
      for (size_t i=parameters.freeze;i<mo_ket_.size();i++) {
	for(size_t j=i;j<mo_ket_.size();j++){
	  real_function_6d Qftitj = make_f_xy(t(i),t(j));
	  apply_Q12(Qftitj);
	  Qftitj.print_size("Q12|t"+stringify(i)+"t"+stringify(j)+">");
	  CC_Pair pair_tmp(Qftitj,i,j);
	  result.insert(pair_tmp.i, pair_tmp.j, pair_tmp);
	}
      }
      time.info();
      return result;
    }

    // right now this is all copied from mp2.cc
    double compute_mp2_pair_energy(CC_Pair &pair) const;

    /// Projectors to project out the occupied space
    // 3D on vector of functions
    void Q(vecfuncT &f) const {
      for (size_t i = 0; i < f.size(); i++) Q(f[i]);
    }
    /// return |\tau_k><k|f_i>
    vecfuncT P(const vecfuncT &f,const CC_vecfunction &tau) const {
      vecfuncT result;
      for (size_t i = 0; i < f.size(); i++)result.push_back(P(f[i],tau));
      return result;
    }
    // 3D on single function
    // use the projector class, like in Q12
    void Q(real_function_3d &f) const {
      for (size_t i = 0; i < mo_ket_.size(); i++) {
	f -= mo_bra_(i).function.inner(f) * mo_ket_(i).function;
      }
    }
    /// returns |tau_k><k|f>
    real_function_3d P(const real_function_3d &f,const CC_vecfunction &tau) const {
      real_function_3d result = real_factory_3d(world);
      for (size_t i = 0; i < mo_ket_.size(); i++) {
	result+= mo_bra_(i).function.inner(f) * tau(i).function;
      }
      return result;
    }

    /// CCSD/CC2 singles potential parts

    /// Genereal function which evaluates a CC_singles potential
    vecfuncT potential_singles(const Pairs<CC_Pair> u,
			       const CC_vecfunction & singles, const potentialtype_s &name) const {
      //output_section("Now doing Singles Potential " + assign_name(name));
      if (singles.functions.size() != mo_ket_.size() - parameters.freeze)
	warning(
	    "Somethings wrong: Size of singles unequal to size of orbitals minus freeze parameter");
      CC_Timer timer(world, assign_name(name));
      CC_data data(name);
      vecfuncT result;

      switch (name) {
	case pot_F3D_:
	  result = fock_residue_closed_shell(singles);
	  break;
	case pot_ccs_:
	  result = ccs_potential(singles);
	  break;
	case pot_S2b_u_:
	  result = S2b_u_part(u, singles);
	  break;
	case pot_S2c_u_:
	  result = S2c_u_part(u, singles);
	  break;
	case pot_S4a_u_:
	  result = S4a_u_part(u, singles);
	  break;
	case pot_S4b_u_:
	  result = S4b_u_part(u, singles);
	  break;
	case pot_S4c_u_:
	  result = S4c_u_part(u, singles);
	  break;
	case pot_S2b_r_:
	  result = S2b_reg_part(singles);
	  break;
	case pot_S2c_r_:
	  result = S2c_reg_part(singles);
	  break;
	case pot_S4a_r_:
	  result = S4a_reg_part(singles);
	  break;
	case pot_S4b_r_:
	  result = S4b_reg_part(singles);
	  break;
	case pot_S4c_r_:
	  result = S4c_reg_part(singles);
	  break;
	case pot_S1_:
	  result = S1(singles);
	  break;
	case pot_S5a_:
	  result = S5a(singles);
	  break;
      }

      truncate(world, result);
      data.result_size = get_size(world, result);
      data.result_norm = norm2(world, result);
      data.time = timer.current_time();
      data.info();
      performance_S.insert(data.name, data);
      if (result.size() != mo_ket_.size() - parameters.freeze)
	warning(
	    "Somethings wrong: Size of singles-potential unequal to size of orbitals minus freeze parameter");
      return result;
    }

    real_function_6d potential_doubles(const CC_Pair &u, const CC_vecfunction &singles,
				       const potentialtype_d &name) const {
      CC_Timer timer(world, assign_name(name));
      CC_data data(assign_name(name));
      output("Now Doing " + assign_name(name) + " \n\n");

      real_function_6d result = real_factory_6d(world);

      switch (name) {
	//		case pot_D6b_D8b_D9_:
	//			result = D6b_D8b_D9(taui, tauj, singles);
	//			break;
	//		case pot_D4b_D6c_D8a_:
	//			result = D4b_D6c_D8a(taui, tauj, singles);
	//			break;
	case pot_F6D_:
	  result = fock_residue_6d(u);
	  break;
	case pot_cc2_coulomb_:
	  result = make_cc2_coulomb_parts(singles(u.i),singles(u.j),singles);
	  break;
	case pot_cc2_residue_:
	  result = make_cc2_residue(singles(u.i),singles(u.j));
	  break;
	default:
	  error(
	      "unknown or unsupported key for doubles potential: "
	      + assign_name(name));
	  break;
      }

      result.print_size(assign_name(name));
      output("Finished with " + assign_name(name));
      data.result_norm = result.norm2();
      data.result_size = get_size(result);
      data.time = (timer.current_time());
      performance_D.insert(data.name, data);
      return result;
    }

    // The Fock operator is partitioned into F = T + Vn + R
    // the fock residue R= 2J-K for closed shell is computed here
    // J_i = \sum_k <k|r12|k> |tau_i>
    // K_i = \sum_k <k|r12|tau_i> |k>
    vecfuncT fock_residue_closed_shell(const CC_vecfunction &tau) const;

    /// brilloin terms of ccs
    vecfuncT S1(const CC_vecfunction &tau) const;
    vecfuncT S5a(const CC_vecfunction &tau) const;

    /// The CCS Potential without Brillouin terms and Fock residue
    vecfuncT ccs_potential(const CC_vecfunction &tau) const;
    vecfuncT ccs_potential_new(const CC_vecfunction &tau)const;

    // result: \sum_k( 2<k|g|uik>_2 - <k|g|uik>_1 )
    vecfuncT S2b_u_part(const Pairs<CC_Pair> &doubles,
			const CC_vecfunction &singles) const;

    // result: -\sum_k( <l|kgi|ukl>_2 - <l|kgi|ukl>_1)
    vecfuncT S2c_u_part(const Pairs<CC_Pair> &doubles,
			const CC_vecfunction &singles) const;

    /// The Part of the CC2 singles potential which depends on singles and doubles (S4a, S4b, S4c)
    vecfuncT S4a_u_part(const Pairs<CC_Pair> &doubles,
			const CC_vecfunction &singles) const;

    // result: -\sum_k( <l|kgtaui|ukl>_2 - <l|kgtaui|ukl>_1) | kgtaui = <k|g|taui>
    vecfuncT S4b_u_part(const Pairs<CC_Pair> &doubles,
			const CC_vecfunction &singles) const;

    vecfuncT S4c_u_part(const Pairs<CC_Pair> &doubles,
			const CC_vecfunction &singles) const;

    vecfuncT S2b_reg_part(const CC_vecfunction &singles) const;

    vecfuncT S2c_reg_part(const CC_vecfunction &singles) const;

    vecfuncT S4a_reg_part(const CC_vecfunction &singles) const;

    /// result: -\sum_{kl}( 2 <l|kgtaui|Qftktl> - <l|kgtaui|Qftltk>
    /// this is the same as S2c with taui instead of i
    vecfuncT S4b_reg_part(const CC_vecfunction &singles) const;

    /// result: 4<l|kgtauk|Qftitl> - 2<l|kgtauk|Qftlti> - 2<k|lgtauk|Qftitl> + <k|lgtauk|Qftlti>
    vecfuncT S4c_reg_part(const CC_vecfunction &singles) const;

    /// CC2 singles diagrams with 6d functions as input
    /// Use GFInterface in function_interface.h as kernel (f*g) and do not reconstruct \tau = f12u(1,2) if possible
    /// Since the correlation factor of CC2 has Slater form like in MP2: g12f12 = g12(1-exp(-mu*r12)/r12) = g12 - exp(-mu*r12)/r12 = Coulomb_Operator - BSH_Operator(mu)


    // CC2 Doubles Potential

    /// smalll helper function to track the time for the projetor
    void apply_Q12(real_function_6d &f,
		   const std::string &msg = "6D-function") const {
      CC_Timer Q12_time(world, "Applying Q12 to " + msg);
      f = Q12(f);
      Q12_time.info();
    }

    /// Make the CC2 Residue which is:  Q12f12(T-eij + 2J -K +Un )|titj> + Q12Ue|titj> - [K,f]|titj>  with |ti> = |\tau i>+|i>
    // @param[in] \tau_i which will create the |t_i> = |\tau_i>+|i> intermediate
    // @param[in] \tau_j
    /// \todo Parameter descriptions.
    /// @return Equation: \f$ Q12f12(T-eij + 2J -K +Un )|titj> + Q12Ue|titj> - [K,f]|titj> \f$  with \f$ |ti> = |\tau i>+|i> \f$
    /// Right now Calculated in the decomposed form: \f$ |titj> = |i,j> + |\tau i,\tau j> + |i,\tau j> + |\tau i,j> \f$
    /// The G_Q_Ue and G_Q_KffK part which act on |ij> are already calculated and stored as constant_term in u (same as for MP2 calculations) -> this should be the biggerst (faster than |titj> form)
    real_function_6d make_cc2_residue(const CC_function &taui,
				      const CC_function &tauj) const;

    // apply the kinetic energy operator to a decomposed 6D function
    /// @param[in] y a 3d function x (will be particle 1 in the decomposed 6d function)
    /// @param[in] x a 3d function y (will be particle 2 in the decomposed 6d function)
    /// @return a 6d function: G(f12*T*|xy>)
    real_function_6d make_GQfT_xy(const real_function_3d &x,
				  const real_function_3d &y, const size_t &i, const size_t &j) const;

    // MP2 Terms are
    // fock_residue_6d = (2J - Kn + Un) |u_{ij}> , KR = R12^{-1}*K*R12 (nuclear tranformed K)
    // Uen|ij> = R12{-1}*U_e*R12 |ij>

    /// The 6D Fock residue on the cusp free pair function u_{ij}(1,2) is: (2J - Kn - Un)|u_{ij}>
    real_function_6d fock_residue_6d(const CC_Pair &u) const;


    /// Exchange Operator on 3D function
    /// !!!!Prefactor (-1) is not included
    real_function_3d K(const CC_function &f) const;
    real_function_3d K(const real_function_3d &f) const;

    /// Exchange Operator on Pair function: -(K(1)+K(2))u(1,2)
    /// if i==j in uij then the symmetry will be exploited
    /// !!!!Prefactor (-1) is not included here!!!!
    real_function_6d K(const real_function_6d &u,
		       const bool symmetric = false, const double thresh = FunctionDefaults<6>::get_thresh()) const;

    /// Exchange Operator on Pair function: -(K(1)+K(2))u(1,2)
    /// K(1)u(1,2) = \sum_k <k(3)|g13|u(3,2)> |k(1)>
    /// 1. X(3,2) = bra_k(3)*u(3,2)
    /// 2. Y(1,2) = \int X(3,2) g13 d3
    /// 3. result = Y(1,2)*ket_k(1)
    /// !!!!Prefactor (-1) is not included here!!!!
    real_function_6d apply_K(const real_function_6d &u,
			     const size_t &particle, const double thresh = FunctionDefaults<6>::get_thresh()) const;

    /// returns \sum_k (<k|g|f> *|k>).truncate()
    real_function_3d apply_K(const CC_function &f) const;

    /// Apply Ue on a tensor product of two 3d functions: Ue(1,2) |x(1)y(2)> (will be either |ij> or |\tau_i\tau_j> or mixed forms)
    /// The Transformed electronic regularization potential (Kutzelnigg) is R_{12}^{-1} U_e R_{12} with R_{12} = R_1*R_2
    /// It is represented as: R_{12}^{-1} U_e R_{12} = U_e + R^-1[Ue,R]
    /// where R^-1[Ue,R] = R^-1 [[T,f],R] (see: Regularizing the molecular potential in electronic structure calculations. II. Many-body
    /// methods, F.A.Bischoff)
    /// The double commutator can be evaluated as follows:  R^-1[[T,f],R] = -Ue_{local}(1,2)*(Un_{local}(1) - Un_{local}(2))
    /// @param[in] x the 3D function for particle 1
    /// @param[in] y the 3D function for particle 2
    /// @param[in] i the first index of the current pair function (needed to construct the BSH operator for screening)
    /// @param[in] j the second index of the current pair function
    /// @return  R^-1U_eR|x,y> the transformed electronic smoothing potential applied on |x,y> :
    real_function_6d apply_transformed_Ue(const CC_function &x,
					  const CC_function &y) const;

    /// Apply the Exchange Commutator [K,f]|xy>
    real_function_6d apply_exchange_commutator(const CC_function &x,
					       const CC_function &y, const double thresh = FunctionDefaults<6>::get_thresh()) const;

    /// Apply the Exchange operator on a tensor product multiplied with f12
    /// !!! Prefactor of (-1) is not inclued in K here !!!!
    real_function_6d apply_Kf(const CC_function &x, const CC_function &y, const double thresh = FunctionDefaults<6>::get_thresh()) const;

    /// Apply fK on a tensor product of two 3D functions
    /// fK|xy> = fK_1|xy> + fK_2|xy>
    /// @param[in] x the first 3D function in |xy>, structure holds index i and type (HOLE, PARTICLE, MIXED, UNDEFINED)
    /// @param[in] y the second 3D function in |xy>  structure holds index i and type (HOLE, PARTICLE, MIXED, UNDEFINED)
    real_function_6d apply_fK(const CC_function &x, const CC_function &y, const double thresh = FunctionDefaults<6>::get_thresh()) const;

    real_function_3d apply_laplacian(const real_function_3d &x) const;

    vecfuncT apply_F(const CC_vecfunction &x) const;

    real_function_3d apply_F(const CC_function &x) const;

    /// little helper function to pack a vector of CC_3D_functions (just structures which hold the function the index and the type)
    std::vector<CC_function> make_CC_3D_function(const vecfuncT &f,
						 const functype &type) {
      std::vector<CC_function> result(f.size());
      for (size_t i = 0; i < f.size(); i++) {
	CC_function tmp(f[i], i, type);
	result[i] = tmp;
      }
      return result;
    }

    // gives back \epsilon_{ij} = \epsilon_i + \epsilon_j
    double get_epsilon(const size_t &i, const size_t &j) const {
      return (orbital_energies[i] + orbital_energies[j]);
    }
    // gives back the orbital energies
    std::vector<double> get_orbital_energies() const {
      return orbital_energies;
    }
    /// swap particles 1 and 2

    /// param[in] all CC_Pairs
    /// param[in] the i index
    /// param[in] the j index
    /// param[out] a 6d function correspoding to electron pair ij
    /// if i>j the pair will be created via: fij(1,2) = fji(2,1)
    real_function_6d get_pair_function(const Pairs<CC_Pair> &pairs,
				       const size_t i, const size_t j) const {
      if (i > j) {
	const real_function_6d & function = pairs(j, i).function;
	const real_function_6d & swapped_function = swap_particles(
	    function);
	return swapped_function;

      } else {
	return pairs(i, j).function;
      }
    }

    /// param[in]	f	a function of 2 particles f(1,2)
    /// return	the input function with particles swapped g(1,2) = f(2,1)
    real_function_6d swap_particles(const real_function_6d& f) const;

    // Calculate the CC2 energy equation which is
    // \omega = \sum_{ij} 2<ij|g|\tau_{ij}> - <ij|g|\tau_{ji}> + 2 <ij|g|\tau_i\tau_j> - <ij|g|\tau_j\tau_i>
    // with \tau_{ij} = u_{ij} + Q12f12|ij> + Q12f12|\tau_i,j> + Q12f12|i,\tau_j> + Q12f12|\tau_i\tau_j>
    double get_CC2_correlation_energy() const;
    double compute_ccs_correlation_energy(const CC_function &taui,
					  const CC_function &tauj) const;
    double compute_cc2_pair_energy(const CC_Pair &u, const CC_function &taui,
				   const CC_function &tauj) const;
    /// Calculate the integral <bra1,bra2|gQf|ket1,ket2>
    // the bra elements are always the R2orbitals
    // the ket elements can be \tau_i , or orbitals dependet n the type given
    double make_ij_gQf_ij(const size_t &i, const size_t &j, CC_Pair &u) const;
    double make_ijgQfxy(const size_t &i, const size_t &j, const CC_function &x,
			const CC_function &y) const;
    double make_ijgfxy(const size_t &i, const size_t &j,
		       const real_function_3d &x, const real_function_3d &y) const;
    /// Make two electron integral (expensive without intermediates) use just for debugging
    double make_ijgxy(const size_t &i, const size_t &j,
		      const real_function_3d &x, const real_function_3d &y) const;
    double make_integral(const size_t &i, const size_t &j, const CC_function &x,
			 const CC_function&y) const;
    /// Make two electron integral with the pair function
    double make_ijgu(const size_t &i, const size_t &j, const CC_Pair &u) const;
    double make_ijgu(const size_t &i, const size_t &j,
		     const real_function_6d &u) const;
    /// Make two electron integral with BSH operator
    double make_ijGu(const size_t &i, const size_t &j, const CC_Pair &u) const;
    /// apply the operator \f$ gf = 1/(2\gamma)*(Coulomb - 4\pi*BSH_\gamma) \f$
    /// works only if f = (1-exp(-\gamma*r12))/(2\gamma)
    real_function_3d apply_gf(const real_function_3d &f) const;

    real_function_3d apply_f12(const CC_function & bra,
			       const CC_function &ket) const {
      if (bra.type != HOLE) {
	output("Apply_f12, bra state is no hole state");
	return (*f12op)(bra.function * ket.function);
      }
      if (ket.type == HOLE) {
	return intermediates_.get_fEX(bra, ket);
      } else if (ket.type == PARTICLE) {
	return intermediates_.get_pfEX(bra, ket);
      } else if (ket.type == MIXED) {
	return intermediates_.get_fEX(bra, ket)
	    + intermediates_.get_pfEX(bra, ket);
      } else if (ket.type == RESPONSE){
	return intermediates_.get_rEX(bra,ket);
      } else {
	output("Apply_f12, ket state undefined");
	return (*f12op)(bra.function * ket.function);
      }
    }

    real_function_3d apply_g12(const CC_function & bra,
			       const CC_function &ket) const {
      if (bra.type != HOLE) {
	output("Apply_g12, bra state is no hole state");
	return (*poisson)(bra.function * ket.function);
      }
      if (ket.type == HOLE) {
	return intermediates_.get_EX(bra, ket);
      } else if (ket.type == PARTICLE) {
	return intermediates_.get_pEX(bra, ket);
      } else if (ket.type == MIXED) {
	return intermediates_.get_EX(bra, ket) + intermediates_.get_pEX(bra, ket);
      } else if (ket.type == RESPONSE){
	return intermediates_.get_rEX(bra,ket);
      } else {
	output("Apply_g12, ket state undefined");
	return (*poisson)(bra.function * ket.function);
      }
    }
    /// returns <k|g|x>*|y>
    real_function_3d make_kgx_y(const CC_function &k, const CC_function &x, const CC_function &y)const{
      return (apply_g12(k,x)*y.function);
    }
    /// returns <k|f|x>*|y>
    real_function_3d make_kfx_y(const CC_function &k, const CC_function &x, const CC_function &y)const{
      return (apply_f12(k,x)*y.function);
    }


    /// @param[in] x Function which is convoluted with (it is assumed that x is already multiplied with R2)
    /// @param[in] y function over which is not integrated
    /// @param[in] z function which is correlated with y over Q12f12
    /// @return <x(2)|Q12f12|y(1)z(2)>_2
    /// Calculation is done in 4 steps over: Q12 = 1 - O1 - O2 + O12
    /// 1. <x|f12|z>*|y>
    /// 2. - \sum_m mxfyz |m>
    /// 3. - \sum_m <x|m>*mfz*|y>
    /// 4. +\sum_{mn} <x|m> nmfyz |n>
    /// Description: Similar to convolute x_gQf_yz just without coulomb operator
    real_function_3d convolute_x_Qf_yz(const CC_function &x,
				       const CC_function &y, const CC_function &z) const;

    /// Doubles potentials
    /// G_D4b = G(Q12\sum_k <k|g|j>(1) |i\tau_k> + <k|g|i>(2) |\tau_k,j>)
    /// use that Q12 = Q1*Q2
    /// need to apply G to every single term in order to avoid entangelment


    real_function_6d make_xy(const CC_function &x, const CC_function &y) const;

    real_function_6d make_f_xy(const CC_function &x,
			       const CC_function &y,const double thresh = FunctionDefaults<6>::get_thresh()) const;

    real_function_6d make_f_xy_screened(const CC_function &x,
					const CC_function &y, const real_convolution_6d &screenG) const;


  private:

    /// The World
    World &world;
    /// Nemo
    const Nemo &nemo;
    /// Thresh for the bsh operator
    double bsh_eps = std::min(FunctionDefaults<6>::get_thresh(), 1.e-4);
    /// Electronic correlation factor
    CorrelationFactor corrfac;
    /// All necessary parameters
    const CC_Parameters &parameters;
    /// The ket and the bra element of the occupied space
    /// if a  nuclear correlation factor is used the bra elements are the MOs multiplied by the squared nuclear correlation factor (done in the constructor)
    const CC_vecfunction mo_bra_;
    const CC_vecfunction mo_ket_;
    /// The orbital energies
    const std::vector<double> orbital_energies;
    std::vector<double> init_orbital_energies(const Nemo &nemo) const {
      std::vector<double> eps;
      if (world.rank() == 0)
	std::cout << "SCF Orbital Energies are:\n";
      for (size_t i = 0; i < mo_ket_.size(); i++) {
	eps.push_back(nemo.get_calc()->aeps(i));
	if (world.rank() == 0)
	  std::cout << nemo.get_calc()->aeps(i);
      }
      if (world.rank() == 0)
	std::cout << "\n" << std::endl;
      return eps;
    }
    /// Helper function to initialize the const mo_bra and ket elements
    CC_vecfunction make_mo_bra(const Nemo &nemo) const {
      vecfuncT tmp = mul(world, nemo.nuclear_correlation->square(),
			 nemo.get_calc()->amo);
      set_thresh(world, tmp, parameters.thresh_3D);
      CC_vecfunction mo_bra(tmp, HOLE);
      return mo_bra;
    }

    CC_vecfunction make_mo_ket(const Nemo&nemo) const {
      vecfuncT tmp = nemo.get_calc()->amo;
      set_thresh(world, tmp, parameters.thresh_3D);
      CC_vecfunction mo_ket(tmp, HOLE);
      return mo_ket;
    }
    /// The poisson operator (Coulomb Operator)
    std::shared_ptr<real_convolution_3d> poisson = std::shared_ptr
	< real_convolution_3d
	> (CoulombOperatorPtr(world, parameters.lo,
			      parameters.thresh_poisson));
    /// The BSH Operator for the f12g12 convolution which is with f12= 1/(2gamma)[1-exp(-gamma*r12)], f12g12 = 1/(2gamma) [CoulombOp - BSHOp(gamma)]
    std::shared_ptr<real_convolution_3d> fBSH = std::shared_ptr
	< real_convolution_3d
	> (BSHOperatorPtr3D(world, corrfac.gamma(), parameters.lo,
			    parameters.thresh_poisson));
    /// The f12 convolution operator
    std::shared_ptr<real_convolution_3d> f12op = std::shared_ptr
	< real_convolution_3d
	> (SlaterF12OperatorPtr(world, corrfac.gamma(), parameters.lo,
				parameters.thresh_poisson));
    /// Intermediates (some need to be refreshed after every iteration)
    CC_Intermediates intermediates_;
    /// The current singles potential (Q\sum singles_diagrams) , needed for application of the fock opeerator on a singles function
    vecfuncT current_singles_potential;
    /// The 6D part of S2c and S2b (only the parts which depends on the u-function, not the regularization tail (since the singles change)
    mutable vecfuncT current_s2b_u_part;
    mutable vecfuncT current_s2c_u_part;
    StrongOrthogonalityProjector<double, 3> Q12;

  public:
    void check_stored_singles_potentials() {
      output("Removing stored singles potentials\n");
      if(not current_s2b_u_part.empty())  warning("S2b_u_part is not empty before singles iteration ... if this is the first macro-iteration this is OK");
      if(not current_s2c_u_part.empty())  warning("S2b_u_part is not empty before singles iteration ... if this is the first macro-iteration this is OK");
      if(not current_singles_potential.empty()) warning("Singles_potential is not empty before singles iteration ... if this is the first macro-iteration this is OK");
    }
    void remove_stored_singles_potentials() {
      output("Removing stored singles potentials\n");
      current_s2b_u_part.clear();
      current_s2c_u_part.clear();
      current_singles_potential.clear();
    }

    //	// Screen Potential
    //	screeningtype screen_potential(const CC_vecfunction &singles,const CC_function &ti, const CC_function &tj, const potentialtype_d &name)const{
    //		size_t i = ti.i;
    //		size_t j = tj.j;
    //		CC_function moi(mo_ket_[i],i,HOLE);
    //		CC_function moj(mo_ket_[j],j,HOLE);
    //		double estimate=0.0;
    //
    //		switch(name);
    //		{
    //		case _D6b_ :
    //			for(auto k:singles.functions){
    //				for(auto l:singles.functions){
    //					estimate += fabs(intermediates_.get_integral(mo_ket_[k.i],mo_ket_[l.i],moi,moj))*k.function.norm2()*l.function.norm2();
    //				}
    //			}
    //		case _D6c_:
    //			for(auto k:singles.functions){
    //				estimate +=  (intermediates_.get_EX(k.i,i).norm2()*k.function.norm2()*tj.function.norm2()
    //		                     +intermediates_.get_EX(k.i,j).norm2()*k.function.norm2()*ti.function.norm2()
    //							 +intermediates_.get_pEX(k.i,i).norm2()*k.function.norm2()*mo_ket_[i].norm2()
    //							 +intermediates_.get_pEX(k.i,j).norm2()*k.function.norm2()*mo_ket_[j].norm2());
    //			}
    //		case _D8a_:
    //
    //		case _D8b_:
    //
    //		case _D9_:
    //
    //		}
    //		estimate = fabs(estimate);
    //		if(estimate>thresh_6D) return _calculate_;
    //		else if(estimate>thresh_6D_tight) return _refine_;
    //		else return _neglect_;
    //	}

    void screening(const real_function_3d &x, const real_function_3d &y) const {
      double normx = x.norm2();
      double normy = y.norm2();
      double norm_xy = normx * normy;
      if (world.rank() == 0)
	std::cout << "Screening |xy> 6D function, norm is: " << norm_xy
	<< std::endl;
      //return norm_xy;
    }

    double guess_thresh(const CC_function &x, const CC_function &y) const {
      double norm = x.function.norm2() * y.function.norm2();
      double thresh = parameters.thresh_6D;
      if (norm > parameters.thresh_6D)
	thresh = parameters.thresh_6D;
      else thresh = parameters.tight_thresh_6D;
      return thresh;
    }

    // Debug function, content changes from time to time
    void test_potentials(const int k, const double thresh =
	FunctionDefaults<3>::get_thresh(), const bool refine = true) const {
      output_section("Little Debug and Testing Session");
      // testing laplace operator with different thresholds
      const size_t old_k = FunctionDefaults<3>::get_k();
      {
	CC_Timer time(world, "time");
	FunctionDefaults<3>::set_thresh(thresh);
	FunctionDefaults<3>::set_k(k);
	std::vector < std::shared_ptr<real_derivative_3d> > gradop;
	gradop = gradient_operator<double, 3>(world);
	std::string name = "_" + stringify(k) + "_" + stringify(thresh);
	if (world.rank() == 0)
	  std::cout
	  << "Testing Laplace operator with threshold  "
	  + stringify(FunctionDefaults<3>::get_thresh())
	  + " and k="
	  + stringify(FunctionDefaults<3>::get_k())
	  + " and refinement=" + stringify(refine) + "\n";

	real_function_3d gauss = real_factory_3d(world).f(f_gauss);
	real_function_3d laplace_gauss_analytical =
	    real_factory_3d(world).f(f_laplace_gauss);
	real_function_3d laplace_gauss_analytical_old_k = project(
	    laplace_gauss_analytical, old_k);
	plot_plane(world, gauss, "gauss" + name);
	plot_plane(world, laplace_gauss_analytical,
		   "laplace_gauss_analytical_old_k" + name);

	real_function_3d laplace_gauss_numerical = real_factory_3d(world);
	for (size_t i = 0; i < 3; i++) {
	  real_function_3d tmp = (*gradop[i])(gauss);
	  real_function_3d tmp2 = (*gradop[i])(tmp);
	  laplace_gauss_numerical += tmp2;
	}

	real_function_3d laplace_gauss_diff = laplace_gauss_analytical
	    - laplace_gauss_numerical;
	plot_plane(world, laplace_gauss_diff, "laplace_gauss_diff" + name);
	laplace_gauss_diff.print_size(
	    "||laplace on gauss num and ana      ||");

	real_function_3d projected_numerical = project(
	    laplace_gauss_numerical, old_k);
	real_function_3d laplace_gauss_diff2 =
	    laplace_gauss_analytical_old_k - projected_numerical;
	plot_plane(world, laplace_gauss_diff2,
		   "laplace_gauss_diff_old_k" + name);
	laplace_gauss_diff.print_size(
	    "||laplace on gauss num and ana old k||");

	FunctionDefaults<3>::set_thresh(parameters.thresh_3D);
	FunctionDefaults<3>::set_k(old_k);
	world.gop.fence();
	time.info();
      }
    }

    real_function_3d smooth_function(const real_function_3d &f,
				     const size_t mode) const {
      size_t k = f.get_impl()->get_k();
      real_function_3d fproj = project(f, k - 1);
      real_function_3d freproj = project(fproj, k);
      real_function_3d smoothed2 = 0.5 * (f + freproj);
      // double diff = (freproj - f).norm2();
      // double diff2 = (smoothed2 - f).norm2();
      // if(world.rank()==0) std::cout << "||f - f_smoothed|| =" << diff << std::endl;
      // if(world.rank()==0) std::cout << "||f - f_smoothed2||=" << diff2 << std::endl;
      if (mode == 1)
	return freproj;
      else if (mode == 2)
	return smoothed2;
      else {
	std::cout << "Unknown smoothing mode, returning unsmoothed function"
	    << std::endl;
	return f;
      }
    }

    // return <f|F-eps|f> matrix
    Tensor<double> make_reduced_fock_matrix(const CC_vecfunction &f, const double eps)const{
      vecfuncT Ff = apply_F(f);
      vecfuncT fbra = mul(world,nemo.nuclear_correlation->square(),f.get_vecfunction());
      vecfuncT epsf = f.get_vecfunction();
      scale(world,epsf,eps);
      Tensor<double> result(f.size(),f.size());
      for(size_t i=0;i<Ff.size();i++){
	for(size_t j=0;j<fbra.size();j++){
	  const double eji = fbra[j].inner(Ff[i]) - fbra[j].inner(epsf[i]);
	  result(j,i) = eji;
	}
      }
      return result;
    }


    template<size_t NDIM>
    void test_greens_operators(const double thresh, const size_t k, const double eps, const double bsh_thresh = 1.e-6)const{
      std::cout << "\n\nTesting " << NDIM << "-dimensional Greens Operator with thresh=" << thresh << " BSH-thresh=" << bsh_thresh << " and epsilon=" << eps<< std::endl;
      FunctionDefaults<NDIM>::set_k(k);
      FunctionDefaults<NDIM>::set_thresh(thresh);
      Function<double,NDIM> f = FunctionFactory<double,NDIM>(world).f(gauss_ND<NDIM>);
      f.print_size("TestGaussFunction");
      //Function<double,NDIM> one = FunctionFactory<double,NDIM>(world).f(unitfunction<NDIM>);
      SeparatedConvolution<double,NDIM> G = BSHOperator<NDIM>(world, sqrt(-2.0 * eps),1.e-6, 1.e-5);
      Function<double,NDIM> Lf = general_apply_laplacian<NDIM>(f);
      Lf.print_size("Laplace(f)");
      // Helmholtz eq: (Delta + 2eps)f = ...
      Lf += 2.0*eps*f;
      Lf.truncate();
      Lf.print_size("(Laplace +2eps)f");
      Function<double,NDIM> GLf= G(Lf);

      //const double integral_f = one.inner(f);
      //const double integral_GLf = one.inner(GLf);

      //std::cout << "integral(f)  =" << integral_f   << std::endl;
      //std::cout << "integral(GLf)=" << integral_GLf << std::endl;
      std::cout << "<f|f>     = " << f.inner(f)     << std::endl;
      std::cout << "<GLf|f>   = " << GLf.inner(f)   << std::endl;
      std::cout << "<f|GLf>   = " << f.inner(GLf)   << std::endl;
      std::cout << "<GLf|GLf> = " << GLf.inner(GLf) << std::endl;
      std::cout << "\n\n";


    }



    template<size_t NDIM>
    Function<double,NDIM> general_apply_laplacian(const Function<double,NDIM> &f)const{
      Function<double,NDIM> laplace_f = FunctionFactory<double,NDIM>(world);
      for (int axis = 0; axis < NDIM; ++axis) {
	Derivative<double,NDIM> D = free_space_derivative<double, NDIM>(world,axis);
	const Function<double,NDIM> Df = D(f);
	const Function<double,NDIM> D2f= D(Df).truncate();
	laplace_f += D2f;
      }
      return laplace_f;
    }

    double size_of(const CC_vecfunction &f)const{
      double size = 0.0;
      for(const auto &tmp:f.functions){
	size += get_size<double,3>(tmp.second.function);
      }
      return size;
    }

    double size_of(const Pairs<CC_Pair> &pairs)const{
      double size =0.0;
      for(const auto & pair:pairs.allpairs){
	size += get_size<double,6>(pair.second.function);
      }
      return size;
    }

    double size_of(const intermediateT &im)const{
      double size=0.0;
      for(const auto & tmp:im.allpairs){
	size += get_size<double,3>(tmp.second);
      }
      return size;
    }

    void print_memory_information(const CC_vecfunction &singles, const Pairs<CC_Pair> &doubles)const{
      const double singles_size = size_of(singles);
      const double doubles_size = size_of(doubles);
      const double intermediates_size = print_size_of_intermediates();
      const double all = singles_size + doubles_size + intermediates_size;
      if(world.rank()==0){
	std::cout << "Singles" << singles_size    <<" (GByte)"<< std::endl;
	std::cout << "Doubles" << doubles_size    <<" (GByte)"<< std::endl;
	std::cout << "-------" << ""              <<" (GByte)"<< std::endl;
	std::cout << "all    " << all             <<" (GByte)"<< std::endl;
	std::cout << "\n----------------------------------\n";
      }
    }
    double print_size_of_intermediates()const{
      const double mo_ket_size  = size_of(mo_ket_);
      const double mo_bra_size  = size_of(mo_bra_);
      const double imEX = size_of(intermediates_.get_EX());
      const double impEX = size_of(intermediates_.get_pEX());
      const double imfEX = size_of(intermediates_.get_fEX());
      const double impfEX = size_of(intermediates_.get_pfEX());
      const double imrEX = size_of(intermediates_.get_rEX());
      const double imrfEX = size_of(intermediates_.get_rfEX());
      const double potJ = get_size<double,3>(intermediates_.get_hartree_potential());
      const double potpJ = get_size<double,3>(intermediates_.get_perturbed_hartree_potential());
      const double rho = get_size<double,3>(intermediates_.get_density());
      const double prho = get_size<double,3>(intermediates_.get_perturbed_density());

      const double all =  mo_ket_size + mo_bra_size + imEX + impEX + imfEX + impfEX + potJ + potpJ + rho + prho;

      if(world.rank()==0){
	std::cout << "\n\n--------Intermediates-Memory-Information--------\n";
	std::cout << "Hole   " << mo_ket_size     <<" (GByte)"<< std::endl;
	std::cout << "R2 Hole" << mo_bra_size     <<" (GByte)"<< std::endl;
	std::cout << "<H|g|H>" << imEX+potJ       <<" (GByte)"<< std::endl;
	std::cout << "<H|g|P>" << impEX+potpJ     <<" (GByte)"<< std::endl;
	std::cout << "<H|f|H>" << imfEX           <<" (GByte)"<< std::endl;
	std::cout << "<H|f|P>" << impfEX          <<" (GByte)"<< std::endl;
	if(imrEX>0.0)  std::cout << "<H|f|R>" << imrEX           <<" (GByte)"<< std::endl;
	if(imrfEX>0.0) std::cout << "<H|f|R>" << imrfEX          <<" (GByte)"<< std::endl;
	std::cout << "density" << rho+prho        <<" (GByte)"<< std::endl;
      }
      return all;
    }

    void plot(const CC_vecfunction &vf)const{
      for(const auto& tmp:vf.functions){
	plot(tmp.second);
      }
    }
    void plot(const CC_function &f)const{
      plot_plane(world,f.function,f.name());
    }
    void plot(const Pairs<CC_Pair> &pairs)const{
      for(const auto& tmp:pairs.allpairs){
	plot(tmp.second);
      }
    }
    void plot(const CC_Pair &f)const{
      plot_plane(world,f.function,f.name());
    }

  };



} /* namespace madness */

#endif /* CCOPERATORS_H_ */
