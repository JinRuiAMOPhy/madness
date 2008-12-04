#define WORLD_INSTANTIATE_STATIC_TEMPLATES  
#include <mra/mra.h>
#include <mra/operator.h>
#include <constants.h>

/// \file heat2.cc
/// \brief Example Green function for the 3D heat equation with a linear term

using namespace madness;

typedef Vector<double,3> coordT;
typedef SharedPtr< FunctionFunctorInterface<double,3> > functorT;
typedef Function<double,3> functionT;
typedef FunctionFactory<double,3> factoryT;
typedef SeparatedConvolution<double,3> operatorT;
typedef Tensor<double> tensorT;

static const double L = 10;     // Half box size
static const long k = 8;        // wavelet order
static const double thresh = 1e-6; // precision
static const double c = 2.0;       // 
static const double tstep = 0.333;
static const double alpha = 1.9; // Exponent 
static const double VVV = 1.0;  // Vp constant value

/// Initial Gaussian with exponent alpha
static double uinitial(const coordT& r) {
    const double x=r[0], y=r[1], z=r[2];
    return exp(-alpha*(x*x+y*y+z*z))*pow(constants::pi/alpha,-1.5);
}


static double Vp(const coordT& r) {
    return VVV;
}


/// Exact solution at time t
class uexact : public FunctionFunctorInterface<double,3> {
    double t;
public:
    uexact(double t) : t(t) {}

    double operator()(const coordT& r) const {
        const double x=r[0], y=r[1], z=r[2];
        double rsq = (x*x+y*y+z*z);
        
        return exp(VVV*t)*exp(-rsq*alpha/(1.0+4.0*alpha*t*c)) * pow(alpha/((1+4*alpha*t*c)*constants::pi),1.5);
    }
};


template<typename T, int NDIM>
struct unaryexp {
    void operator()(const Key<NDIM>& key, Tensor<T>& t) const {
        UNARY_OPTIMIZED_ITERATOR(T, t, *_p0 = exp(*_p0););
    }
    template <typename Archive>
    void serialize(Archive& ar) {}
};

int main(int argc, char** argv) {
    initialize(argc, argv);
    World world(MPI::COMM_WORLD);
    
    startup(world, argc, argv);
    cout.precision(6);

    FunctionDefaults<3>::set_k(k);
    FunctionDefaults<3>::set_thresh(thresh);
    FunctionDefaults<3>::set_refine(true);
    FunctionDefaults<3>::set_autorefine(false);
    FunctionDefaults<3>::set_cubic_cell(-L, L);
    
    functionT u0 = factoryT(world).f(uinitial);
    u0.truncate();

    double u0_norm = u0.norm2();
    double u0_trace = u0.trace();

    if (world.rank() == 0) print("Initial norm", u0_norm,"trace", u0_trace);
    world.gop.fence();

    // du(x,t)/dt = c del^2 u(x,t) + Vp(x)*u(x,t)
    //
    // If Vp=0 time evolution operator is G(x,t) = 1/sqrt(4 pi c t)  exp(-x^2 / 4 c t)
    //
    // Trotter approx for non-zero Vp is
    //
    // G(x,t/2) exp(Vp*t) G(x,t/2)

    // Make exponential of Vp
    functionT expVp = factoryT(world).f(Vp);
    expVp.scale(tstep);
    expVp.unaryop(unaryexp<double,3>());

    print("Vp(0)", expVp(coordT(0.0)));
    
    // Make G(x,t/2)
    tensorT expnt(1), coeff(1);
    expnt[0] = 1.0/(4.0*c*tstep*0.5);
    coeff[0] = pow(4.0*constants::pi*c*tstep*0.5,-1.5);
    operatorT G(world, k, coeff, expnt);

    // Propagate to time t
    functionT ut2 = apply(G, u0);  ut2.truncate();
    ut2 = ut2*expVp;
    functionT ut = apply(G, ut2); ut.truncate();

    double ut_norm = ut.norm2();
    double ut_trace = ut.trace();
    double err = ut.err(uexact(tstep));

    if (world.rank() == 0) print("Final norm", ut_norm,"trace", ut_trace,"err",err);

    finalize();
    return 0;
}

