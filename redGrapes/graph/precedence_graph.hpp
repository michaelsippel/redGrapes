/* Copyright 2019 Michael Sippel
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <list>
#include <unordered_map>
#include <unordered_set>
#include <memory> // std::unique_ptr<>
#include <stdexcept> // std::runtime_error

#include <chrono>

#include <boost/graph/graph_traits.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/labeled_graph.hpp>
#include <boost/graph/depth_first_search.hpp>
#include <boost/graph/copy.hpp>
#include <boost/graph/reverse_graph.hpp>

#include <redGrapes/graph/recursive_graph.hpp>
#include <iostream>

namespace redGrapes
{

struct AllSequential
{
    template <typename ID>
    static bool
    is_serial( ID, ID )
    {
        return true;
    }
};

struct AllParallel
{
    template <typename ID>
    static bool
    is_serial( ID, ID )
    {
        return false;
    }
};

/**
 * Base class
 */
template <
    typename T,
    template <class> typename Graph = DefaultGraph
>
class PrecedenceGraph : public RecursiveGraph<T, Graph>
{
    public:
        using typename RecursiveGraph<T, Graph>::VertexID;

        /// remove edges which don't satisfy the precedence policy
        auto remove_out_edges(VertexID vertex, std::function<bool(T const&)> const & pred)
        {
            std::vector<VertexID> vertices;

            for(auto it = boost::out_edges(vertex, this->graph()); it.first != it.second; ++it.first)
            {
                auto other_vertex = boost::target(*(it.first), this->graph());
                auto & other = graph_get(other_vertex, this->graph());
                if( pred( other.first ) )
                    vertices.push_back( other_vertex );
            }

            for( auto other_vertex : vertices )
                boost::remove_edge(vertex, other_vertex, this->graph());

            return vertices;
        }
}; // class PrecedenceGraph

/**
 * Precedence-graph generated from a queue
 * using an enqueue-policy
 */
template<
    typename T,
    typename EnqueuePolicy,
    template <class> typename Graph = DefaultGraph
>
class QueuedPrecedenceGraph
    : public PrecedenceGraph<T, Graph>
{
    public:
        using VertexID = typename PrecedenceGraph<T, Graph>::VertexID;

        QueuedPrecedenceGraph()
        {}

        QueuedPrecedenceGraph( std::weak_ptr<RecursiveGraph<T, Graph>> parent_graph, VertexID parent_vertex )
        {
            this->parent_graph = parent_graph;
            this->parent_vertex = parent_vertex;
        }

        auto push(T a)
        {
            if( auto graph = this->parent_graph.lock() )
            {
                auto parent_lock = graph->shared_lock();
                EnqueuePolicy::assert_superset( graph_get(this->parent_vertex, graph->graph()).first, a );
            }

            VertexID v = boost::add_vertex( std::make_pair(a, std::shared_ptr<RecursiveGraph<T,Graph>>(nullptr)), this->graph() );

            bool end = true;
            for(auto b : this->queue)
            {
                T const & prop = graph_get(b.first, this->graph()).first;
                if( EnqueuePolicy::is_serial(prop, a) )
                {
                    boost::add_edge(b.first, v, this->graph());
                    if( b.second )
                        break;
                }
                else
                {
                    end = false;
                }
            }

            this->queue.insert(this->queue.begin(), std::make_pair(v, end));

            return v;
        }

        auto update_vertex(VertexID a)
        {
            return this->remove_out_edges(a, [this,a](T const & b){ return !EnqueuePolicy::is_serial(graph_get(a, this->graph()).first, b); } );
	}

        void finish(VertexID vertex)
        {
            boost::clear_vertex( vertex, this->graph() );
            boost::remove_vertex( vertex, this->graph() );

            auto it = std::find_if(this->queue.begin(), this->queue.end(), [vertex](auto x){ return x.first == vertex; });
            if (it != this->queue.end())
                this->queue.erase(it);
            else
                throw std::runtime_error("Queuedprecedencegraph: removed element not in queue");
        }

private:
    std::list<std::pair<VertexID, bool>> queue;
}; // class QueuedPrecedenceGraph

} // namespace redGrapes           
