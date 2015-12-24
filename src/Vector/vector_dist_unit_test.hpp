/*
 * vector_dist_unit_test.hpp
 *
 *  Created on: Mar 6, 2015
 *      Author: Pietro Incardona
 */

#ifndef VECTOR_DIST_UNIT_TEST_HPP_
#define VECTOR_DIST_UNIT_TEST_HPP_

#include <random>
#include "Vector/vector_dist.hpp"

BOOST_AUTO_TEST_SUITE( vector_dist_test )

BOOST_AUTO_TEST_CASE( vector_dist_ghost )
{
	// Communication object
	Vcluster & v_cl = *global_v_cluster;

	typedef Point_test<float> p;
	typedef Point<2,float> s;

	// Get the default minimum number of sub-sub-domain per processor (granularity of the decomposition)
	size_t n_sub = vector_dist<2,float, Point_test<float>, CartDecomposition<2,float> >::getDefaultNsubsub() * v_cl.getProcessingUnits();
	// Convert the request of having a minimum n_sub number of sub-sub domain into grid decompsition of the space
	size_t sz = CartDecomposition<2,float>::getDefaultGrid(n_sub);

	//! [Create a vector of elements distributed on a grid like way]

	Box<2,float> box({0.0,0.0},{1.0,1.0});
	size_t g_div[]= {sz,sz};

	// number of particles
	size_t np = sz * sz;

	// Calculate the number of elements this processor is going to obtain
	size_t p_np = np / v_cl.getProcessingUnits();

	// Get non divisible part
	size_t r = np % v_cl.getProcessingUnits();

	// Get the offset
	size_t offset = v_cl.getProcessUnitID() * p_np + std::min(v_cl.getProcessUnitID(),r);

	// Distribute the remain elements
	if (v_cl.getProcessUnitID() < r)
		p_np++;

	// Create a grid info
	grid_sm<2,void> g_info(g_div);

	// Calculate the grid spacing
	Point<2,float> spacing = box.getP2();
	spacing = spacing / g_div;

	// middle spacing
	Point<2,float> m_spacing = spacing / 2;

	// set the ghost based on the radius cut off (make just a little bit smaller than the spacing)
	Ghost<2,float> g(spacing.get(0) - spacing .get(0) * 0.0001);

	// Boundary conditions
	size_t bc[2]={NON_PERIODIC,NON_PERIODIC};

	// Vector of particles
	vector_dist<2,float, Point_test<float>, CartDecomposition<2,float> > vd(g_info.size(),box,bc,g);

	// size_t
	size_t cobj = 0;

	grid_key_dx_iterator_sp<2> it(g_info,offset,offset+p_np-1);
	auto v_it = vd.getIterator();

	while (v_it.isNext() && it.isNext())
	{
		auto key = it.get();
		auto key_v = v_it.get();

		// set the particle position

		vd.template getPos<s::x>(key_v)[0] = key.get(0) * spacing[0] + m_spacing[0];
		vd.template getPos<s::x>(key_v)[1] = key.get(1) * spacing[1] + m_spacing[1];

		cobj++;

		++v_it;
		++it;
	}

	//! [Create a vector of elements distributed on a grid like way]

	// Both iterators must signal the end, and the number of elements in the vector, must the equal to the
	// predicted one
	BOOST_REQUIRE_EQUAL(v_it.isNext(),false);
	BOOST_REQUIRE_EQUAL(it.isNext(),false);
	BOOST_REQUIRE_EQUAL(cobj,p_np);

	//! [Redistribute the particles and sync the ghost properties]

	// redistribute the particles according to the decomposition
	vd.map();

	v_it = vd.getIterator();

	while (v_it.isNext())
	{
		auto key = v_it.get();

		// fill with the processor ID where these particle live
		vd.template getProp<p::s>(key) = vd.getPos<s::x>(key)[0] + vd.getPos<s::x>(key)[1] * 16;
		vd.template getProp<p::v>(key)[0] = v_cl.getProcessUnitID();
		vd.template getProp<p::v>(key)[1] = v_cl.getProcessUnitID();
		vd.template getProp<p::v>(key)[2] = v_cl.getProcessUnitID();

		++v_it;
	}

	// do a ghost get
	vd.template ghost_get<p::s,p::v>();

	//! [Redistribute the particles and sync the ghost properties]

	// Get the decomposition
	const auto & dec = vd.getDecomposition();

	// Get the ghost external boxes
	openfpm::vector<size_t> vb(dec.getNEGhostBox());

	// Get the ghost iterator
	auto g_it = vd.getGhostIterator();

	size_t n_part = 0;

	// Check if the ghost particles contain the correct information
	while (g_it.isNext())
	{
		auto key = g_it.get();

		// Check the received data
		BOOST_REQUIRE_EQUAL(vd.getPos<s::x>(key)[0] + vd.getPos<s::x>(key)[1] * 16,vd.template getProp<p::s>(key));

		bool is_in = false;
		size_t b = 0;
		size_t lb = 0;

		// check if the received data is in one of the ghost boxes
		for ( ; b < dec.getNEGhostBox() ; b++)
		{
			if (dec.getEGhostBox(b).isInside(vd.getPos<s::x>(key)) == true)
			{
				is_in = true;

				// Add
				vb.get(b)++;
				lb = b;
			}
		}
		BOOST_REQUIRE_EQUAL(is_in,true);

		// Check that the particle come from the correct processor
		BOOST_REQUIRE_EQUAL(vd.getProp<p::v>(key)[0],dec.getEGhostBoxProcessor(lb));

		n_part++;
		++g_it;
	}

	BOOST_REQUIRE(n_part != 0);

    CellDecomposer_sm<2,float> cd(SpaceBox<2,float>(box),g_div,0);

	for (size_t i = 0 ; i < vb.size() ; i++)
	{
		// Calculate how many particle should be in the box
		size_t n_point = cd.getGridPoints(dec.getEGhostBox(i)).getVolumeKey();

		BOOST_REQUIRE_EQUAL(n_point,vb.get(i));
	}
}

void print_test_v(std::string test, size_t sz)
{
	if (global_v_cluster->getProcessUnitID() == 0)
		std::cout << test << " " << sz << "\n";
}

long int decrement(long int k, long int step)
{
	if (k <= 32)
	{
		return 1;
	}
	else if (k - 2*step+1 <= 0)
	{
		return k - 32;
	}
	else
		return step;
}

BOOST_AUTO_TEST_CASE( vector_dist_iterator_test_use_2d )
{
	typedef Point<2,float> s;

	Vcluster & v_cl = *global_v_cluster;

    // set the seed
	// create the random generator engine
	std::srand(v_cl.getProcessUnitID());
    std::default_random_engine eg;
    std::uniform_real_distribution<float> ud(0.0f, 1.0f);

    long int k = 524288 * v_cl.getProcessingUnits();

	long int big_step = k / 30;
	big_step = (big_step == 0)?1:big_step;

	print_test_v( "Testing 2D vector k<=",k);

	// 2D test
	for ( ; k >= 2 ; k-= decrement(k,big_step) )
	{
		BOOST_TEST_CHECKPOINT( "Testing 2D vector k=" << k );

		//! [Create a vector of random elements on each processor 2D]

		Box<2,float> box({0.0,0.0},{1.0,1.0});

		// Boundary conditions
		size_t bc[2]={NON_PERIODIC,NON_PERIODIC};

		vector_dist<2,float, Point_test<float>, CartDecomposition<2,float> > vd(k,box,bc,Ghost<2,float>(0.0));

		auto it = vd.getIterator();

		while (it.isNext())
		{
			auto key = it.get();

			vd.template getPos<s::x>(key)[0] = ud(eg);
			vd.template getPos<s::x>(key)[1] = ud(eg);

			++it;
		}

		vd.map();

		//! [Create a vector of random elements on each processor 2D]

		// Check if we have all the local particles
		size_t cnt = 0;
		const CartDecomposition<2,float> & ct = vd.getDecomposition();
		it = vd.getIterator();

		while (it.isNext())
		{
			auto key = it.get();

			// Check if local
			BOOST_REQUIRE_EQUAL(ct.isLocal(vd.template getPos<s::x>(key)),true);

			cnt++;

			++it;
		}

		//
		v_cl.sum(cnt);
		v_cl.execute();
		BOOST_REQUIRE_EQUAL((long int)cnt,k);
	}
}

BOOST_AUTO_TEST_CASE( vector_dist_iterator_test_use_3d )
{
	typedef Point<3,float> s;

	Vcluster & v_cl = *global_v_cluster;

    // set the seed
	// create the random generator engine
	std::srand(v_cl.getProcessUnitID());
    std::default_random_engine eg;
    std::uniform_real_distribution<float> ud(0.0f, 1.0f);

    long int k = 524288 * v_cl.getProcessingUnits();

	long int big_step = k / 30;
	big_step = (big_step == 0)?1:big_step;

	print_test_v( "Testing 3D vector k<=",k);

	// 3D test
	for ( ; k >= 2 ; k-= decrement(k,big_step) )
	{
		BOOST_TEST_CHECKPOINT( "Testing 3D vector k=" << k );

		//! [Create a vector of random elements on each processor 3D]

		Box<3,float> box({0.0,0.0,0.0},{1.0,1.0,1.0});

		// Boundary conditions
		size_t bc[3]={NON_PERIODIC,NON_PERIODIC,NON_PERIODIC};

		vector_dist<3,float, Point_test<float>, CartDecomposition<3,float> > vd(k,box,bc,Ghost<3,float>(0.0));

		auto it = vd.getIterator();

		while (it.isNext())
		{
			auto key = it.get();

			vd.template getPos<s::x>(key)[0] = ud(eg);
			vd.template getPos<s::x>(key)[1] = ud(eg);
			vd.template getPos<s::x>(key)[2] = ud(eg);

			++it;
		}

		vd.map();

		//! [Create a vector of random elements on each processor 3D]

		// Check if we have all the local particles
		size_t cnt = 0;
		const CartDecomposition<3,float> & ct = vd.getDecomposition();
		it = vd.getIterator();

		while (it.isNext())
		{
			auto key = it.get();

			// Check if local
			BOOST_REQUIRE_EQUAL(ct.isLocal(vd.template getPos<s::x>(key)),true);

			cnt++;

			++it;
		}

		//
		v_cl.sum(cnt);
		v_cl.execute();
		BOOST_REQUIRE_EQUAL(cnt,(size_t)k);
	}
}

BOOST_AUTO_TEST_CASE( vector_dist_periodic_test_use_2d )
{
	typedef Point<2,float> s;

	Vcluster & v_cl = *global_v_cluster;

    // set the seed
	// create the random generator engine
	std::srand(v_cl.getProcessUnitID());
    std::default_random_engine eg;
    std::uniform_real_distribution<float> ud(0.0f, 1.0f);

    long int k = 524288 * v_cl.getProcessingUnits();

	long int big_step = k / 30;
	big_step = (big_step == 0)?1:big_step;

	print_test_v( "Testing 2D periodic vector k<=",k);

	// 2D test
	for ( ; k >= 2 ; k-= decrement(k,big_step) )
	{
		BOOST_TEST_CHECKPOINT( "Testing 2D periodic vector k=" << k );

		//! [Create a vector of random elements on each processor 2D]

		Box<2,float> box({0.0,0.0},{1.0,1.0});

		// Boundary conditions
		size_t bc[2]={PERIODIC,PERIODIC};

		// ghost
		Ghost<2,float> ghost(0.05);

		vector_dist<2,float, Point_test<float>, CartDecomposition<2,float> > vd(k,box,bc,ghost);

		auto it = vd.getIterator();

		while (it.isNext())
		{
			auto key = it.get();

			vd.template getPos<s::x>(key)[0] = ud(eg);
			vd.template getPos<s::x>(key)[1] = ud(eg);

			++it;
		}

		vd.map();

		// sync the ghost
		vd.ghost_get<0>();

		//! [Create a vector of random elements on each processor 2D]

		// Check if we have all the local particles
		size_t l_cnt = 0;
		size_t nl_cnt = 0;
		size_t n_out = 0;
		const CartDecomposition<2,float> & ct = vd.getDecomposition();
		it = vd.getIterator();

		Box<2,float> dom_ext = box;
		dom_ext.enlarge(ghost);

		while (it.isNext())
		{
			auto key = it.get();

			// Check if local
			if (ct.isLocalBC(vd.template getPos<s::x>(key),bc) == true)
				l_cnt++;
			else
				nl_cnt++;

			// Check that all particles are inside the Domain + Ghost part
			if (dom_ext.isInside(vd.template getPos<s::x>(key)) == false)
					n_out++;

			++it;
		}

		BOOST_REQUIRE_EQUAL(n_out,0);

		if (k > 524288)
			BOOST_REQUIRE(nl_cnt != 0);

		//
		v_cl.sum(l_cnt);
		v_cl.execute();
		BOOST_REQUIRE_EQUAL((long int)l_cnt,k);
	}
}

BOOST_AUTO_TEST_CASE( vector_dist_periodic_test_use_3d )
{
	typedef Point<3,float> s;

	Vcluster & v_cl = *global_v_cluster;

    // set the seed
	// create the random generator engine
	std::srand(v_cl.getProcessUnitID());
    std::default_random_engine eg;
    std::uniform_real_distribution<float> ud(0.0f, 1.0f);

    long int k = 524288 * v_cl.getProcessingUnits();

	long int big_step = k / 30;
	big_step = (big_step == 0)?1:big_step;

	print_test_v( "Testing 3D periodic vector k<=",k);

	// 3D test
	for ( ; k >= 2 ; k-= decrement(k,big_step) )
	{
		BOOST_TEST_CHECKPOINT( "Testing 3D periodic vector k=" << k );

		//! [Create a vector of random elements on each processor 3D]

		Box<3,float> box({0.0,0.0,0.0},{1.0,1.0,1.0});

		// Boundary conditions
		size_t bc[3]={PERIODIC,PERIODIC,PERIODIC};

		// ghost
		Ghost<3,float> ghost(0.05);

		vector_dist<3,float, Point_test<float>, CartDecomposition<3,float> > vd(k,box,bc,ghost);

		auto it = vd.getIterator();

		while (it.isNext())
		{
			auto key = it.get();

			vd.template getPos<s::x>(key)[0] = ud(eg);
			vd.template getPos<s::x>(key)[1] = ud(eg);
			vd.template getPos<s::x>(key)[2] = ud(eg);

			++it;
		}

		vd.map();

		// sync the ghost
		vd.ghost_get<0>();

		//! [Create a vector of random elements on each processor 3D]

		// Check if we have all the local particles
		size_t l_cnt = 0;
		size_t nl_cnt = 0;
		size_t n_out = 0;
		const CartDecomposition<3,float> & ct = vd.getDecomposition();
		it = vd.getIterator();

		Box<3,float> dom_ext = box;
		dom_ext.enlarge(ghost);

		while (it.isNext())
		{
			auto key = it.get();

			// Check if local
			if (ct.isLocalBC(vd.template getPos<s::x>(key),bc) == true)
				l_cnt++;
			else
				nl_cnt++;

			// Check that all particles are inside the Domain + Ghost part
			if (dom_ext.isInside(vd.template getPos<s::x>(key)) == false)
					n_out++;


			++it;
		}

		BOOST_REQUIRE_EQUAL(n_out,0);

		if (k > 524288)
			BOOST_REQUIRE(nl_cnt != 0);

		//
		v_cl.sum(l_cnt);
		v_cl.execute();
		BOOST_REQUIRE_EQUAL(l_cnt,(size_t)k);
	}
}

BOOST_AUTO_TEST_SUITE_END()

#endif /* VECTOR_DIST_UNIT_TEST_HPP_ */
