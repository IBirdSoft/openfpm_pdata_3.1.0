/*
 * parmetis_util.hpp
 *
 *  Created on: Oct 07, 2015
 *      Author: Antonio Leo
 */

#ifndef PARMETIS_UTIL_HPP
#define PARMETIS_UTIL_HPP

#include <iostream>
#include "parmetis.h"
#include "VTKWriter/VTKWriter.hpp"
#include "VCluster.hpp"
#include "Graph/ids.hpp"

/*! \brief Metis graph structure
 *
 * Metis graph structure
 *
 */
struct Parmetis_graph
{
	//! The number of vertices in the graph
	idx_t * nvtxs;

	//! number of balancing constrains
	//! more practical, are the number of weights for each vertex
	//! PS even we you specify vwgt == NULL ncon must be set at leat to
	//! one
	idx_t * ncon;

	//! For each vertex it store the adjacency lost start for the vertex i
	idx_t * xadj;

	//! For each vertex it store a list of all neighborhood vertex
	idx_t * adjncy;

	//! Array that store the weight for each vertex
	idx_t * vwgt;

	//! Array of the vertex size, basically is the total communication amount
	idx_t * vsize;

	//! The weight of the edge
	idx_t * adjwgt;

	//! number of part to partition the graph
	idx_t * nparts;

	//! Desired weight for each partition (one for each constrain)
	real_t * tpwgts;

	//! For each partition load imbalance tollerated
	real_t * ubvec;

	//! Additional option for the graph partitioning
	idx_t * options;

	//! return the total comunication cost for each partition
	idx_t * objval;

	//! Is a output vector containing the partition for each vertex
	idx_t * part;

	//! Upon successful completion, the number of edges that are cut by the partitioning is written to this parameter.
	idx_t * edgecut;

	//! This parameter describes the ratio of inter-processor communication time compared to data redistri- bution time. It should be set between 0.000001 and 1000000.0. If ITR is set high, a repartitioning with a low edge-cut will be computed. If it is set low, a repartitioning that requires little data redistri- bution will be computed. Good values for this parameter can be obtained by dividing inter-processor communication time by data redistribution time. Otherwise, a value of 1000.0 is recommended.
	real_t * itr;

	//! This is used to indicate the numbering scheme that is used for the vtxdist, xadj, adjncy, and part arrays. (0 for C-style, start from 0 index)
	idx_t * numflag;

	//! This is used to indicate if the graph is weighted. wgtflag can take one of four values:
	// 0 No weights (vwgt and adjwgt are both NULL).
	// 1 Weights on the edges only (vwgt is NULL).
	// 2 Weights on the vertices only (adjwgt is NULL).
	// 3 Weights on both the vertices and edges.
	idx_t * wgtflag;
};

//! Balance communication and computation
#define BALANCE_CC 1
//! Balance communication computation and memory
#define BALANCE_CCM 2
//! Balance computation and comunication and others
#define BALANCE_CC_O(c) c+1

/*! \brief Helper class to define Metis graph
 *
 * \tparam graph structure that store the graph
 *
 */
template<typename Graph>
class Parmetis
{
	// Graph in metis reppresentation
	Parmetis_graph Mg;

	// Original graph
	//	Graph & g;

	// Communticator for OpenMPI
	MPI_Comm comm = NULL;

	// VCluster
	Vcluster & v_cl;

	// Process rank information
	int p_id = 0;

	// nc Number of partition
	size_t nc = 0;

	// first re-mapped id
	rid first;

	// last re-mapped id
	rid last;

	// number of vertices that the processor has
	size_t nvertex;

	/*! \brief Construct Adjacency list
	 *
	 * \param g Global graph
	 *
	 */
	void constructAdjList(Graph &g, const std::unordered_map<rid, gid> & m2g)
	{
		// init basic graph informations and part vector
		// Put the total communication size to NULL

		Mg.nvtxs[0] = nvertex;
		Mg.part = new idx_t[nvertex];

		size_t nedge = 0;
		size_t i = 0;
		for (rid j = first; i < nvertex; i++, ++j)
		{
			Mg.part[i] = p_id;
			nedge += g.getNChilds(m2g.find(j)->second.id);
		}

		// create xadj, adjlist, vwgt, adjwgt and vsize
		Mg.xadj = new idx_t[nvertex + 1];
		Mg.adjncy = new idx_t[nedge];
		Mg.vwgt = new idx_t[nvertex];
		Mg.adjwgt = new idx_t[nedge];
		Mg.vsize = new idx_t[nvertex];

		//! starting point in the adjacency list
		size_t prev = 0;

		// actual position
		size_t id = 0;

		size_t j = 0;

		// for each vertex calculate the position of the starting point in the adjacency list
		for (rid i = first; i <= last; ++i, j++)
		{
			gid idx = m2g.find(i)->second;

			// Add weight to vertex and migration cost
			Mg.vwgt[j] = g.vertex(idx.id).template get<nm_v::computation>();
			Mg.vsize[j] = g.vertex(idx.id).template get<nm_v::migration>();

			// Calculate the starting point in the adjacency list
			Mg.xadj[id] = prev;

			// Create the adjacency list and the weights for edges
			for (size_t s = 0; s < g.getNChilds(idx.id); s++)
			{

				size_t child = g.getChild(idx.id, s);

				Mg.adjncy[prev + s] = g.vertex(child).template get<nm_v::id>();
				Mg.adjwgt[prev + s] = g.getChildEdge(idx.id, s).template get<nm_e::communication>();
			}

			// update the position for the next vertex
			prev += g.getNChilds(idx.id);

			id++;
		}

		// Fill the last point
		Mg.xadj[id] = prev;
	}

public:

	/*! \brief Constructor
	 *
	 * Construct a metis graph from Graph_CSR
	 *
	 * \param g Graph we want to convert to decompose
	 * \param nc number of partitions
	 *
	 */
	Parmetis(Vcluster & v_cl, size_t nc) :
			v_cl(v_cl), nc(nc)
	{
		// TODO Move into VCluster
		MPI_Comm_dup(MPI_COMM_WORLD, &comm);

		// Nullify Mg
		Mg.nvtxs = NULL;
		Mg.ncon = NULL;
		Mg.xadj = NULL;
		Mg.adjncy = NULL;
		Mg.vwgt = NULL;
		Mg.vsize = NULL;
		Mg.adjwgt = NULL;
		Mg.nparts = NULL;
		Mg.tpwgts = NULL;
		Mg.ubvec = NULL;
		Mg.options = NULL;
		Mg.objval = NULL;
		Mg.part = NULL;
		Mg.edgecut = NULL;
		Mg.itr = NULL;
		Mg.numflag = NULL;
		Mg.wgtflag = NULL;
	}

	//TODO deconstruct new variables
	/*! \brief destructor
	 *
	 * Destructor, It destroy all the memory allocated
	 *
	 */
	~Parmetis()
	{
		// Deallocate the Mg structure
		if (Mg.nvtxs != NULL)
		{
			delete[] Mg.nvtxs;
		}

		if (Mg.ncon != NULL)
		{
			delete[] Mg.ncon;
		}

		if (Mg.xadj != NULL)
		{
			delete[] Mg.xadj;
		}

		if (Mg.adjncy != NULL)
		{
			delete[] Mg.adjncy;
		}

		if (Mg.vwgt != NULL)
		{
			delete[] Mg.vwgt;
		}

		if (Mg.adjwgt != NULL)
		{
			delete[] Mg.adjwgt;
		}

		if (Mg.nparts != NULL)
		{
			delete[] Mg.nparts;
		}

		if (Mg.tpwgts != NULL)
		{
			delete[] Mg.tpwgts;
		}

		if (Mg.ubvec != NULL)
		{
			delete[] Mg.ubvec;
		}

		if (Mg.options != NULL)
		{
			delete[] Mg.options;
		}

		if (Mg.part != NULL)
		{
			delete[] Mg.part;
		}

		if (Mg.edgecut != NULL)
		{
			delete[] Mg.edgecut;
		}

		if (Mg.numflag != NULL)
		{
			delete[] Mg.numflag;
		}

		if (Mg.wgtflag != NULL)
		{
			delete[] Mg.wgtflag;
		}

		if (is_openfpm_init() == true)
			MPI_Comm_free(&comm);
	}

	/*! \brief Set the Sub-graph
	 *
	 * \param g Global graph to set
	 * \param w true if vertices have weights
	 */
	void initSubGraph(Graph & g, const openfpm::vector<rid> & vtxdist, const std::unordered_map<rid, gid> & m2g, bool w)
	{
		p_id = v_cl.getProcessUnitID();

		first = vtxdist.get(p_id);
		last = vtxdist.get(p_id + 1) - 1;
		nvertex = last.id - first.id + 1;

		setDefaultParameters(w);

		// construct the adjacency list
		constructAdjList(g, m2g);

		//reset(g, vtxdist, m2g, w);
	}

	/*! \brief Decompose the graph
	 *
	 * \tparam i which property store the decomposition
	 *
	 */
	template<unsigned int i>
	void decompose(const openfpm::vector<rid> & vtxdist)
	{
		// Decompose

		ParMETIS_V3_PartKway((idx_t *) vtxdist.getPointer(), Mg.xadj, Mg.adjncy, Mg.vwgt, Mg.adjwgt, Mg.wgtflag, Mg.numflag, Mg.ncon, Mg.nparts, Mg.tpwgts, Mg.ubvec, Mg.options, Mg.edgecut, Mg.part, &comm);
	}

	/*! \brief Refine the graph
	 *
	 * \tparam i which property store the refined decomposition
	 *
	 */

	template<unsigned int i>
	void refine(openfpm::vector<rid> & vtxdist)
	{
		// Refine

		ParMETIS_V3_RefineKway((idx_t *) vtxdist.getPointer(), Mg.xadj, Mg.adjncy, Mg.vwgt, Mg.adjwgt, Mg.wgtflag, Mg.numflag, Mg.ncon, Mg.nparts, Mg.tpwgts, Mg.ubvec, Mg.options, Mg.edgecut, Mg.part, &comm);
	}

	/*! \brief Get graph partition vector
	 *
	 */
	idx_t* getPartition()
	{
		return Mg.part;
	}

	/*! \brief Reset graph and reconstruct it
	 *
	 * \param g Global graph
	 * \param vtxdist Distribution vector
	 * \param m2g Mapped id to global id map
	 * \param vgw Using weights on vertices
	 */
	void reset(Graph & g, const openfpm::vector<rid> & vtxdist, const std::unordered_map<rid, gid> & m2g, bool vgw)
	{
		first = vtxdist.get(p_id);
		last = vtxdist.get(p_id + 1) - 1;
		nvertex = last.id - first.id + 1;

		// Deallocate the graph structures

		if (Mg.xadj != NULL)
		{
			delete[] Mg.xadj;
		}

		if (Mg.adjncy != NULL)
		{
			delete[] Mg.adjncy;
		}

		if (Mg.vwgt != NULL)
		{
			delete[] Mg.vwgt;
		}

		if (Mg.adjwgt != NULL)
		{
			delete[] Mg.adjwgt;
		}

		if (Mg.part != NULL)
		{
			delete[] Mg.part;
		}

		setDefaultParameters(vgw);

		// construct the adjacency list
		constructAdjList(g, m2g);
	}

	/*! \brief Seth the default parameters for parmetis
	 *
	 *
	 */
	void setDefaultParameters(bool w)
	{
		Mg.nvtxs = new idx_t[1];

		// Set the number of constrains
		Mg.ncon = new idx_t[1];
		Mg.ncon[0] = 1;

		// Set to null the weight of the vertex (init after in constructAdjList) (can be removed)
		Mg.vwgt = NULL;

		// Set to null the weight of the edge (init after in constructAdjList) (can be removed)
		Mg.adjwgt = NULL;

		// Set the total number of partitions
		Mg.nparts = new idx_t[1];
		Mg.nparts[0] = nc;

		//! Set option for the graph partitioning (set as default)

		Mg.options = new idx_t[4];
		Mg.options[0] = 0;
		Mg.options[1] = 0;
		Mg.options[2] = 0;
		Mg.options[3] = 0;

		//! is an output vector containing the partition for each vertex

		//! adaptiveRepart itr value
		Mg.itr = new real_t[1];
		Mg.itr[0] = 1000.0;

		Mg.objval = new idx_t[1];

		//! init tpwgts to have balanced vertices and ubvec

		Mg.tpwgts = new real_t[Mg.nparts[0]];
		Mg.ubvec = new real_t[Mg.nparts[0]];

		for (size_t s = 0; s < (size_t) Mg.nparts[0]; s++)
		{
			Mg.tpwgts[s] = 1.0 / Mg.nparts[0];
			Mg.ubvec[s] = 1.05;
		}

		Mg.edgecut = new idx_t[1];
		Mg.edgecut[0] = 0;

		//! This is used to indicate the numbering scheme that is used for the vtxdist, xadj, adjncy, and part arrays. (0 for C-style, start from 0 index)
		Mg.numflag = new idx_t[1];
		Mg.numflag[0] = 0;

		//! This is used to indicate if the graph is weighted. wgtflag can take one of four values:
		Mg.wgtflag = new idx_t[1];

		if (w)
			Mg.wgtflag[0] = 3;
		else
			Mg.wgtflag[0] = 0;
	}

	const Parmetis<Graph> & operator=(const Parmetis<Graph> & pm)
	{
		comm = pm.comm;
		v_cl = pm.v_cl;
		p_id = pm.p_id;
		nc = pm.nc;

		setDefaultParameters(pm.Mg.wgtflag[0] == 3);

		return *this;
	}

	const Parmetis<Graph> & operator=(Parmetis<Graph> && pm)
	{
		comm = pm.comm;
		v_cl = pm.v_cl;
		p_id = pm.p_id;
		nc = pm.nc;

		setDefaultParameters(pm.Mg.wgtflag[0] == 3);

		return *this;
	}

};

#endif
