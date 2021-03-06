/* Copyright 2019 Michael Sippel
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/**
 * @file redGrapes/graph_util.hpp
 */

#pragma once

#include <akrzemi/optional.hpp>
#include <boost/graph/graph_traits.hpp>

namespace redGrapes
{

/**
 * get vertex-property from vertex-descriptor
 */
template <typename Graph>
typename Graph::vertex_property_type &
graph_get(
    typename boost::graph_traits<Graph>::vertex_descriptor v,
    Graph & graph
)
{
    return boost::get(boost::vertex_bundle, graph)[v];
}

/**
 * find vertex-descriptor from property
 *
 * TODO: overload for boost::labeled_graph
 *
 * @return pair of (vertex-descriptor, true) if vertex with
 *         property exists, else (_, false)
 */
template <typename Graph>
std::experimental::optional< typename boost::graph_traits<Graph>::vertex_descriptor >
graph_find_vertex(
    typename Graph::vertex_property_type a,
    Graph & graph
)
{
    typename boost::graph_traits<Graph>::vertex_iterator it, end;

    for (boost::tie(it, end) = boost::vertices(graph); it != end; ++it)
    {
        if (graph_get(*it, graph) == a)
            return std::experimental::optional< typename boost::graph_traits<Graph>::vertex_descriptor >( *it );
    }

    return std::experimental::nullopt;
}

} // namespace redGrapes
