/*
 * Distribution_unit_tests.hpp
 *
 *  Created on: Feb 27, 2016
 *      Author: i-bird
 */

#ifndef SRC_DECOMPOSITION_DISTRIBUTION_DISTRIBUTION_UNIT_TESTS_HPP_
#define SRC_DECOMPOSITION_DISTRIBUTION_DISTRIBUTION_UNIT_TESTS_HPP_

#include "config.h"
#include "SpaceDistribution.hpp"
#include <unistd.h>

/*! \brief Set a sphere as high computation cost
 *
 * \param dist Distribution structure
 * \param gr grid info
 * \param center of the sphere
 * \param radius radius of the sphere
 * \param max_l maximum load of the processor
 * \param min_l minimum load of the processor
 *
 */
template<unsigned int dim, typename Distribution> void setSphereComputationCosts(Distribution & dist, grid_sm<dim, void> & gr, Point<3, float> center, float radius, size_t max_l, size_t min_l)
{
	float radius2 = radius * radius;
	float eq;

	// Position structure for the single vertex
	float pos[dim];

	for (size_t i = 0; i < dist.getNSubSubDomains(); i++)
	{
		dist.getSubSubDomainPosition(i, pos);

		eq = 0;
		for (size_t j = 0; j < dim; j++)
			eq += (pos[j] - center.get(j)) * (pos[j] - center.get(j));

		if (eq <= radius2)
		{
			dist.setComputationCost(i, max_l);
			dist.setMigrationCost(i, max_l * 2);
		}
		else
		{
			dist.setComputationCost(i, min_l);
			dist.setMigrationCost(i, min_l * 2);
		}

		// set Migration cost and communication cost
		for (size_t j = 0; j < dist.getNSubSubDomainNeighbors(i); j++)
			dist.setCommunicationCost(i, j, 1);
	}
}

BOOST_AUTO_TEST_SUITE (Distribution_test)

BOOST_AUTO_TEST_CASE( Metis_distribution_test)
{
	Vcluster<> & v_cl = create_vcluster();

	if (v_cl.getProcessingUnits() != 3)
	return;

	//! [Initialize a Metis Cartesian graph and decompose]

	// Cartesian grid
	size_t sz[3] = { GS_SIZE, GS_SIZE, GS_SIZE };

	// Box
	Box<3, float> domain( { 0.0, 0.0, 0.0 }, { 1.0, 1.0, 1.0 });

	// Grid info
	grid_sm<3, void> info(sz);

	// Ghost area
	Ghost<3,float> g(0.1);

	// boundary conditions
	size_t bc[3] = {NON_PERIODIC,NON_PERIODIC,NON_PERIODIC};

	CartDecompositionStrategy<3,float> cds(v_cl);
	cds.setParameters(sz,domain,bc,g);
	cds.createCartGraph();

	MetisDistribution<3, float> met_dist(v_cl,cds.getGraph());

	// Set metis on test, It fix the seed (not required if we are not testing)
	met_dist.onTest();

	// Initialize Cart graph and decompose

	met_dist.createCartGraph(info,domain);
	met_dist.distribute();

	BOOST_REQUIRE_EQUAL(met_dist.get_ndec(),1ul);

	//! [Initialize a Metis Cartesian graph and decompose]

	BOOST_REQUIRE(met_dist.getUnbalance() < 0.03);

	if (v_cl.getProcessUnitID() == 0)
	{met_dist.write("vtk_metis_distribution");}

	size_t b = GS_SIZE * GS_SIZE * GS_SIZE / 5;

	//! [Decomposition Metis with weights]

	// Initialize the weights to 1.0
	// not required, if we set ALL Computation,Migration,Communication cost

	// Change set some weight on the graph and re-decompose

	for (size_t k = 0; k < met_dist.getNOwnerSubSubDomains(); k++)
	{
		size_t i = met_dist.getOwnerSubSubDomain(k);

		if (i == 0 || i == b || i == 2*b || i == 3*b || i == 4*b)
			met_dist.setComputationCost(i,10);
		else
			met_dist.setComputationCost(i,1);
	}

	for (size_t i = 0 ; i <  met_dist.getNSubSubDomains() ; i++)
	{
		// We also show how to set some Communication and Migration cost

		met_dist.setMigrationCost(i,1);

		for (size_t j = 0; j < met_dist.getNSubSubDomainNeighbors(i); j++)
		met_dist.setCommunicationCost(i,j,1);
	}

	met_dist.distribute();

	BOOST_REQUIRE_EQUAL(met_dist.get_ndec(),2ul);

	//! [Decomposition Metis with weights]

	BOOST_REQUIRE(met_dist.getUnbalance() < 0.06);

	if (v_cl.getProcessUnitID() == 0)
	{met_dist.write("vtk_metis_distribution_red");}

	// check that match

	bool test;

	if (v_cl.getProcessUnitID() == 0)
	{
	#ifdef HAVE_OSX

		test = compare("0_vtk_metis_distribution.vtk", "src/Decomposition/Distribution/test_data/vtk_metis_distribution_osx_test.vtk");
		BOOST_REQUIRE_EQUAL(true,test);
		test = compare("0_vtk_metis_distribution_red.vtk","src/Decomposition/Distribution/test_data/vtk_metis_distribution_red_osx_test.vtk");
		BOOST_REQUIRE_EQUAL(true,test);

	#elif __GNUC__ == 6 && __GNUC_MINOR__ == 3

		test = compare("0_vtk_metis_distribution.vtk", "src/Decomposition/Distribution/test_data/vtk_metis_distribution_test.vtk");
		BOOST_REQUIRE_EQUAL(true,test);
		test = compare("0_vtk_metis_distribution_red.vtk","src/Decomposition/Distribution/test_data/vtk_metis_distribution_red_test.vtk");
		BOOST_REQUIRE_EQUAL(true,test);

	#endif
	}

	// Copy the Metis distribution

	MetisDistribution<3, float> met_dist2(v_cl,cds.getGraph());

	met_dist2 = met_dist;

	test = (met_dist2 == met_dist);

	BOOST_REQUIRE_EQUAL(test,true);

	// We fix the size of MetisDistribution if you are gointg to change this number
	// please check the following
	// duplicate functions
	// swap functions
	// Copy constructors
	// operator= functions
	// operator== functions

//	BOOST_REQUIRE_EQUAL(sizeof(MetisDistribution<3,float>),720ul);
}

BOOST_AUTO_TEST_CASE( Parmetis_distribution_test)
{
	Vcluster<> & v_cl = create_vcluster();

	if (v_cl.getProcessingUnits() != 3)
	return;

	//! [Initialize a ParMetis Cartesian graph and decompose]

	size_t div[3] = { GS_SIZE, GS_SIZE, GS_SIZE };

	size_t bc[3] = {NON_PERIODIC,NON_PERIODIC,NON_PERIODIC};

	// Physical domain
	Box<3, float> domain( { 0.0, 0.0, 0.0 }, { 10.0, 10.0, 10.0 });

	Ghost<3,float> g(0.1);

	// Grid info
	grid_sm<3, void> info(div);

	CartDecompositionStrategy<3,float> cds(v_cl);
	cds.setParameters(div,domain,bc,g);
	cds.createCartGraph();

	ParMetisDistribution<3, float> pmet_dist(v_cl,cds.getGraph());

	// First create the center of the weights distribution, check it is coherent to the size of the domain
	Point<3, float> center( { 2.0, 2.0, 2.0 });

	// It produces a sphere of radius 2.0
	// with high computation cost (5) inside the sphere and (1) outside
	setSphereComputationCosts(pmet_dist, info, center, 2.0f, 5ul, 1ul);

	// first decomposition
	pmet_dist.distribute();

	BOOST_REQUIRE_EQUAL(pmet_dist.get_ndec(),1ul);

	//! [Initialize a ParMetis Cartesian graph and decompose]

	if (v_cl.getProcessUnitID() == 0)
	{
		// write the first decomposition
		pmet_dist.write("vtk_parmetis_distribution_0");

#ifdef HAVE_OSX

		bool test = compare(std::to_string(v_cl.getProcessUnitID()) + "_vtk_parmetis_distribution_0.vtk","src/Decomposition/Distribution/test_data/" + std::to_string(v_cl.getProcessUnitID()) + "_vtk_parmetis_distribution_0_osx_test.vtk");
		BOOST_REQUIRE_EQUAL(true,test);

#else

        bool test = compare(std::to_string(v_cl.getProcessUnitID()) + "_vtk_parmetis_distribution_0.vtk","src/Decomposition/Distribution/test_data/" + std::to_string(v_cl.getProcessUnitID()) + "_vtk_parmetis_distribution_0_test.vtk");
        BOOST_REQUIRE_EQUAL(true,test);

#endif
	}

	//! [refine with parmetis the decomposition]

	float stime = 0.0, etime = 10.0, tstep = 0.1;

	// Shift of the sphere at each iteration
	Point<3, float> shift( { tstep, tstep, tstep });

	size_t iter = 1;
	size_t n_dec = 1;

	for(float t = stime; t < etime; t = t + tstep, iter++)
	{
		if(t < etime/2)
		center += shift;
		else
		center -= shift;

		setSphereComputationCosts(pmet_dist, info, center, 2.0f, 5, 1);

		// With some regularity refine and write the parmetis distribution
		if ((size_t)iter % 10 == 0)
		{
			pmet_dist.refine();
			n_dec++;
			BOOST_REQUIRE_EQUAL(pmet_dist.get_ndec(),n_dec);

			if (v_cl.getProcessUnitID() == 0)
			{
				std::stringstream str;
				str << "vtk_parmetis_distribution_" << iter;
				pmet_dist.write(str.str());

#ifdef HAVE_OSX

				// Check
				bool test = compare(std::to_string(v_cl.getProcessUnitID()) + "_" + str.str() + ".vtk", "src/Decomposition/Distribution/test_data/" + std::to_string(v_cl.getProcessUnitID()) + "_" + str.str() + "_osx_test.vtk");
				BOOST_REQUIRE_EQUAL(true,test);

#else

				// Check
				bool test = compare(std::to_string(v_cl.getProcessUnitID()) + "_" + str.str() + ".vtk", "src/Decomposition/Distribution/test_data/" + std::to_string(v_cl.getProcessUnitID()) + "_" + str.str() + "_test.vtk");
				BOOST_REQUIRE_EQUAL(true,test);

#endif
			}
		}
	}

	//! [refine with parmetis the decomposition]

//	BOOST_REQUIRE_EQUAL(sizeof(ParMetisDistribution<3,float>),872ul);
}

BOOST_AUTO_TEST_CASE( DistParmetis_distribution_test)
{
	Vcluster<> & v_cl = create_vcluster();

	if (v_cl.getProcessingUnits() != 3)
		return;

	//! [Initialize a ParMetis Cartesian graph and decompose]

	DistParMetisDistribution<3, float> pmet_dist(v_cl);

	// Physical domain
	Box<3, float> box( { 0.0, 0.0, 0.0 }, { 10.0, 10.0, 10.0 });

	// Grid info
	grid_sm<3, void> info( { GS_SIZE, GS_SIZE, GS_SIZE });

	// Initialize Cart graph and decompose
	pmet_dist.createCartGraph(info, box);

	// First create the center of the weights distribution, check it is coherent to the size of the domain
	Point<3, float> center( { 2.0, 2.0, 2.0 });

	// It produces a sphere of radius 2.0
	// with high computation cost (5) inside the sphere and (1) outside
	setSphereComputationCosts(pmet_dist, info, center, 2.0f, 5ul, 1ul);

	// first decomposition
	pmet_dist.decompose();

	//! [Initialize a ParMetis Cartesian graph and decompose]

	// write the first decomposition
	pmet_dist.write("vtk_dist_parmetis_distribution_0");

	// Check
	if (v_cl.getProcessUnitID() == 0)
	{

	#ifdef HAVE_OSX

		bool test = compare("vtk_dist_parmetis_distribution_0.vtk","src/Decomposition/Distribution/test_data/vtk_dist_parmetis_distribution_0_osx_test.vtk");
		BOOST_REQUIRE_EQUAL(true,test);

	#else

		bool test = compare("vtk_dist_parmetis_distribution_0.vtk","src/Decomposition/Distribution/test_data/vtk_dist_parmetis_distribution_0_test.vtk");
		BOOST_REQUIRE_EQUAL(true,test);

	#endif

	}

	//! [refine with dist_parmetis the decomposition]

	float stime = 0.0, etime = 10.0, tstep = 0.1;

	// Shift of the sphere at each iteration
	Point<3, float> shift( { tstep, tstep, tstep });

	size_t iter = 1;

	for(float t = stime; t < etime; t = t + tstep, iter++)
	{
		if(t < etime/2)
		center += shift;
		else
		center -= shift;

		setSphereComputationCosts(pmet_dist, info, center, 2.0f, 5, 1);

		// With some regularity refine and write the parmetis distribution
		if ((size_t)iter % 10 == 0)
		{
			pmet_dist.refine();

			std::stringstream str;
			str << "vtk_dist_parmetis_distribution_" << iter;
			pmet_dist.write(str.str());

			// Check
			if (v_cl.getProcessUnitID() == 0)
			{
#ifdef HAVE_OSX
				bool test = compare(str.str() + ".vtk",std::string("src/Decomposition/Distribution/test_data/") + str.str() + "_osx_test.vtk");
				BOOST_REQUIRE_EQUAL(true,test);

#else

				bool test = compare(str.str() + ".vtk",std::string("src/Decomposition/Distribution/test_data/") + str.str() + "_test.vtk");
				BOOST_REQUIRE_EQUAL(true,test);

#endif

			}
		}
	}

	//! [refine with dist_parmetis the decomposition]
}

BOOST_AUTO_TEST_CASE( Space_distribution_test)
{
	Vcluster<> & v_cl = create_vcluster();

	if (v_cl.getProcessingUnits() != 3)
		return;

	//! [Initialize a Space Cartesian graph and decompose]

	SpaceDistribution<3, float> space_dist(v_cl);

	// Physical domain
	Box<3, float> box( { 0.0, 0.0, 0.0 }, { 10.0, 10.0, 10.0 });

	// Grid info
	grid_sm<3, void> info( { 17, 17, 17 });

	// Initialize Cart graph and decompose
	space_dist.createCartGraph(info,box);

	// first decomposition
	space_dist.decompose();

	//! [Initialize a Space Cartesian graph and decompose]

	if (v_cl.getProcessUnitID() == 0)
	{
		// write the first decomposition
		space_dist.write("vtk_dist_space_distribution_0");

		bool test = compare(std::to_string(v_cl.getProcessUnitID()) + "_vtk_dist_space_distribution_0.vtk","src/Decomposition/Distribution/test_data/" + std::to_string(v_cl.getProcessUnitID()) + + "_vtk_dist_space_distribution_0_test.vtk");
		BOOST_REQUIRE_EQUAL(true,test);
	}

	//! [refine with dist_parmetis the decomposition]
}


BOOST_AUTO_TEST_SUITE_END()

#endif /* SRC_DECOMPOSITION_DISTRIBUTION_DISTRIBUTION_UNIT_TESTS_HPP_ */
