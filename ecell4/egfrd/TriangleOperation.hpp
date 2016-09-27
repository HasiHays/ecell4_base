#ifndef GFRD_POLYGON_TRIANGLE_OPERATION
#define GFRD_POLYGON_TRIANGLE_OPERATION
#include "Vector3Operation.hpp"
#include "circular_iteration.hpp"
#include <boost/array.hpp>
#include <algorithm>
#include <cassert>

template<typename coordT>
inline coordT centroid(const boost::array<coordT, 3>& vertices)
{
    return (vertices[0] + vertices[1] + vertices[2]) * (1e0 / 3e0);
}

template<typename coordT>
inline coordT incenter(const boost::array<coordT, 3>& vertices)
{
    typedef typename scalar_type_helper<coordT>::type valueT;
    const valueT a = length(vertices[2] - vertices[1]);
    const valueT b = length(vertices[0] - vertices[2]);
    const valueT c = length(vertices[1] - vertices[0]);
    const valueT abc = a + b + c;
    return (vertices[0] * a + vertices[1] * b + vertices[2] * c) * (1e0 / abc);
}

template<typename coordT>
inline coordT incenter(const boost::array<coordT, 3>& vertices,
                       const boost::array<coordT, 3>& edges)
{
    typedef typename scalar_type_helper<coordT>::type valueT;
    const valueT a = length(edges[1]);
    const valueT b = length(edges[2]);
    const valueT c = length(edges[0]);
    const valueT abc = a + b + c;
    return (vertices[0] * a + vertices[1] * b + vertices[2] * c) * (1e0 / abc);
}

template<typename coordT>
inline coordT incenter(const boost::array<coordT, 3>& vertices,
    const boost::array<typename scalar_type_helper<coordT>::type, 3>& length_of_edge)
{
    typedef typename scalar_type_helper<coordT>::type valueT;
    const valueT a = length_of_edge[1];
    const valueT b = length_of_edge[2];
    const valueT c = length_of_edge[0];
    const valueT abc = a + b + c;
    return (vertices[0] * a + vertices[1] * b + vertices[2] * c) * (1e0 / abc);
}

template<typename coordT>
inline std::size_t match_edge(const coordT& vec,
                              const boost::array<coordT, 3>& edges)
{
    for(std::size_t i=0; i<3; ++i)
        if(is_same_vec(vec, edges[i])) return i;
    throw std::invalid_argument("not match any edge");
}

template<typename coordT>
coordT
project_to_plane(const coordT& pos, const boost::array<coordT, 3>& vertices,
                 const coordT& normal)
{
    typedef typename scalar_type_helper<coordT>::type valueT;
    assert(std::abs(length(normal) - 1.0) < 1e-10);
    const valueT distance = dot_product(normal, pos - vertices.front());
    return pos - (normal * distance);
}

template<typename coordT>
std::pair<typename scalar_type_helper<coordT>::type, // distance
          typename scalar_type_helper<coordT>::type> // r of circle in triangle
distance(const coordT& pos, const boost::array<coordT, 3>& vertices)
{
    typedef typename scalar_type_helper<coordT>::type valueT;
    // this implementation is from Real-Time Collision Detection by Christer Ericson,
    // published by Morgan Kaufmann Publishers, (c) 2005 Elsevier Inc.
    // pp.141-142

    coordT const& a = vertices[0];
    coordT const& b = vertices[1];
    coordT const& c = vertices[2];

    // Check if P in vertex region outside A
    coordT ab = b - a;
    coordT ac = c - a;
    coordT ap = pos - a;
    valueT d1 = dot_product(ab, ap);
    valueT d2 = dot_product(ac, ap);
    if (d1 <= 0.0 && d2 <= 0.0)
        return std::make_pair(length(pos - a), 0.0); // barycentric coordinates (1,0,0)
    // Check if P in vertex region outside B
    coordT bp = pos - b;
    valueT d3 = dot_product(ab, bp);
    valueT d4 = dot_product(ac, bp);
    if (d3 >= 0.0 && d4 <= d3)
        return std::make_pair(length(pos - b), 0.0); // barycentric coordinates (0,1,0)
    // Check if P in edge region of AB, if so return projection of P onto AB
    valueT vc = d1*d4 - d3*d2;
    if (vc <= 0.0 && d1 >= 0.0 && d3 <= 0.0)
    {
        valueT v = d1 / (d1 - d3);
        return std::make_pair(length(a + ab * v- pos), 0.0); // barycentric coordinates (1-v,v,0)
    }
    // Check if P in vertex region outside C
    coordT cp = pos - c;
    valueT d5 = dot_product(ab, cp);
    valueT d6 = dot_product(ac, cp);
    if (d6 >= 0.0 && d5 <= d6)
        return std::make_pair(length(c - pos), 0.0); // barycentric coordinates (0,0,1)

    //Check if P in edge region of AC, if so return projection of P onto AC
    valueT vb = d5*d2 - d1*d6;
    if (vb <= 0.0 && d2 >= 0.0 && d6 <= 0.0)
    {
        valueT w = d2 / (d2 - d6);
        return std::make_pair(length(a + ac * w - pos), 0.0); // barycentric coordinates (1-w,0,w)
    }
    // Check if P in edge region of BC, if so return projection of P onto BC
    valueT va = d3*d6 - d5*d4;
    if (va <= 0.0 && (d4 - d3) >= 0.0 && (d5 - d6) >= 0.0)
    {
        valueT w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        return std::make_pair(length(b + (c - b) * w - pos), 0.0); // barycentric coordinates (0,1-w,w)
    }
    // P inside face region. Compute Q through its barycentric coordinates (u,v,w)
    valueT denom = 1.0 / (va + vb + vc);
    valueT v = vb * denom;
    valueT w = vc * denom;
    return std::make_pair(length(a + ab * v + ac * w - pos), 0.0); // = u*a + v*b + w*c, u = va * denom = 1.0f-v-w
}

template<typename coordT>
std::pair<bool, coordT> // pair of (whether pierce), pierce point
is_pierce(const coordT& begin, const coordT& end,
          const boost::array<coordT, 3>& vertices)
{
    typedef typename scalar_type_helper<coordT>::type valueT;
    // this implementation is from Real-Time Collision Detection by Christer Ericson,
    // published by Morgan Kaufmann Publishers, (c) 2005 Elsevier Inc.
    // pp.190-194

    const coordT line = begin - end;
    const coordT ab = vertices[1] - vertices[0];
    const coordT ac = vertices[2] - vertices[0];
    const coordT normal = cross_product(ab, ac);

    const valueT d = dot_product(line, normal);
    if(d < 0.0)
        return std::make_pair(false, coordT(0.,0.,0.));

    const coordT ap = begin - vertices[0];
    const valueT t = dot_product(ap, normal);
    if(t < 0.0 || d < t)
        return std::make_pair(false, coordT(0.,0.,0.));

    const coordT e = cross_product(line, ap);
    valueT v = dot_product(ac, e);
    if(v < 0. || d < v)
        return std::make_pair(false, coordT(0.,0.,0.));
    valueT w = -1.0 * dot_product(ab, e);
    if(w < 0. || d < v + w)
        return std::make_pair(false, coordT(0.,0.,0.));

    valueT ood = 1. / d;
    v *= ood;
    w *= ood;
    const valueT u = 1. - v - w;
    boost::array<valueT, 3> bary;
    bary[0] = u;
    bary[1] = v;
    bary[2] = w;
    const coordT intersect = barycentric_to_absolute(bary, vertices);

    return std::make_pair(true, intersect);
}

#endif /* GFRD_POLYGON_TRIANGLE */
