#include "Grid/grid_dist_id.hpp"
#include "data_type/aggregate.hpp"
#include "timer.hpp"

/*!
 * \page Grid_3_gs Grid 3 Gray Scott in 3D
 *
 * # Solving a gray scott-system in 3D # {#e3_gs_gray_scott}
 *
 * This example is just an extension of the 2D Gray scott example.
 * Here we show how to solve a non-linear reaction diffusion system in 3D
 *
 * \see \ref Grid_2_solve_eq
 *
 * \snippet Grid/3_gray_scott/main.cpp constants
 * 
 */

//! \cond [constants] \endcond

constexpr int U = 0;
constexpr int V = 1;

constexpr int x = 0;
constexpr int y = 1;
constexpr int z = 2;


void init(grid_dist_id<3,double,aggregate<double,double> > & Old, grid_dist_id<3,double,aggregate<double,double> > & New, Box<3,double> & domain)
{
	auto it = Old.getDomainIterator();

	while (it.isNext())
	{
		// Get the local grid key
		auto key = it.get();

		// Old values U and V
		Old.template get<U>(key) = 1.0;
		Old.template get<V>(key) = 0.0;

		// Old values U and V
		New.template get<U>(key) = 0.0;
		New.template get<V>(key) = 0.0;

		++it;
	}

	grid_key_dx<3> start({(long int)std::floor(Old.size(0)*1.55f/domain.getHigh(0)),(long int)std::floor(Old.size(1)*1.55f/domain.getHigh(1)),(long int)std::floor(Old.size(1)*1.55f/domain.getHigh(2))});
	grid_key_dx<3> stop ({(long int)std::ceil (Old.size(0)*1.85f/domain.getHigh(0)),(long int)std::ceil (Old.size(1)*1.85f/domain.getHigh(1)),(long int)std::floor(Old.size(1)*1.85f/domain.getHigh(1))});
	auto it_init = Old.getSubDomainIterator(start,stop);

	while (it_init.isNext())
	{
		auto key = it_init.get();

                Old.template get<U>(key) = 0.5 + (((double)std::rand())/RAND_MAX -0.5)/10.0;
                Old.template get<V>(key) = 0.25 + (((double)std::rand())/RAND_MAX -0.5)/20.0;

		++it_init;
	}
}

//! \cond [end fun] \endcond


int main(int argc, char* argv[])
{
	openfpm_init(&argc,&argv);

	// domain
	Box<3,double> domain({0.0,0.0},{2.5,2.5,2.5});
	
	// grid size
	size_t sz[3] = {128,128,128};

	// Define periodicity of the grid
	periodicity<3> bc = {PERIODIC,PERIODIC,PERIODIC};
	
	// Ghost in grid unit
	Ghost<3,long int> g(1);
	
	// deltaT
	double deltaT = 1;

	// Diffusion constant for specie U
	double du = 2*1e-5;

	// Diffusion constant for specie V
	double dv = 1*1e-5;

	// Number of timesteps
        size_t timeSteps = 17000;

	// K and F (Physical constant in the equation)
        double K = 0.065;
        double F = 0.034;

	//! \cond [init lib] \endcond

	/*!
	 * \page Grid_3_gs Grid 3 Gray Scott
	 *
	 * Here we create 2 distributed grid in 2D Old and New. In particular because we want that
	 * the second grid is distributed across processors in the same way we pass the decomposition
	 * of the Old grid to the New one in the constructor with **Old.getDecomposition()**. Doing this,
	 * we force the two grid to have the same decomposition.
	 *
	 * \snippet Grid/3_gray_scott/main.cpp init grid
	 *
	 */

	//! \cond [init grid] \endcond

	grid_dist_id<3, double, aggregate<double,double>> Old(sz,domain,g,bc);

	// New grid with the decomposition of the old grid
        grid_dist_id<3, double, aggregate<double,double>> New(Old.getDecomposition(),sz,g);

	
	// spacing of the grid on x and y
	double spacing[3] = {Old.spacing(0),Old.spacing(1),Old.spacing(2)};

	init(Old,New,domain);

	// sync the ghost
	size_t count = 0;
	Old.template ghost_get<U,V>();

	// because we assume that spacing[x] == spacing[y] we use formula 2
	// and we calculate the prefactor of Eq 2
	double uFactor = deltaT * du/(spacing[x]*spacing[x]);
	double vFactor = deltaT * dv/(spacing[x]*spacing[x]);

	for (size_t i = 0; i < timeSteps; ++i)
	{
		if (i % 300 == 0)
			std::cout << "STEP: " << i << std::endl;

		auto it = Old.getDomainIterator();

		while (it.isNext())
		{
			auto key = it.get();

			// update based on Eq 2
			New.get<U>(key) = Old.get<U>(key) + uFactor * (
										Old.get<U>(key.move(x,1)) +
										Old.get<U>(key.move(x,-1)) +
										Old.get<U>(key.move(y,1)) +
										Old.get<U>(key.move(y,-1)) +
										Old.get<U>(key.move(z,1)) +
										Old.get<U>(key.move(z,-1)) -
										6.0*Old.get<U>(key)) +
										- deltaT * Old.get<U>(key) * Old.get<V>(key) * Old.get<V>(key) +
										- deltaT * F * (Old.get<U>(key) - 1.0);


			// update based on Eq 2
			New.get<V>(key) = Old.get<V>(key) + vFactor * (
										Old.get<V>(key.move(x,1)) +
										Old.get<V>(key.move(x,-1)) +
										Old.get<V>(key.move(y,1)) +
										Old.get<V>(key.move(y,-1)) +
										Old.get<V>(key.move(z,1)) +
                                                                                Old.get<V>(key.move(z,-1)) -
										6*Old.get<V>(key)) +
										deltaT * Old.get<U>(key) * Old.get<V>(key) * Old.get<V>(key) +
										- deltaT * (F+K) * Old.get<V>(key);

			// Next point in the grid
			++it;
		}

		// Here we copy New into the old grid in preparation of the new step
		// It would be better to alternate, but using this we can show the usage
		// of the function copy. To note that copy work only on two grid of the same
		// decomposition. If you want to copy also the decomposition, or force to be
		// exactly the same, use Old = New
		Old.copy(New);

		// After copy we synchronize again the ghost part U and V
		Old.ghost_get<U,V>();

		// Every 30 time step we output the configuration for
		// visualization
		if (i % 60 == 0)
		{
			Old.write("output",count,VTK_WRITER | FORMAT_BINARY);
			count++;
		}
	}
	
	//! \cond [time stepping] \endcond

	/*!
	 * \page Grid_3_gs Grid 3 Gray Scott
	 *
	 * ## Finalize ##
	 *
	 * Deinitialize the library
	 *
	 * \snippet Grid/3_gray_scott/main.cpp finalize
	 *
	 */

	//! \cond [finalize] \endcond

	openfpm_finalize();

	//! \cond [finalize] \endcond
}
