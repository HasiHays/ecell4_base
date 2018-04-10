#include "Barycentric.hpp"

namespace ecell4
{

namespace detail
{

inline Real cross_section(const Barycentric& pos,
                          const Barycentric& displacement,
                          const std::size_t edge_idx)
{
    const std::size_t idx = (edge_idx==0) ? 2 : edge_idx-1;
    return -pos[idx] / displacement[idx];
}

static inline Real triangle_area_2D(const Real x1, const Real y1,
                                    const Real x2, const Real y2,
                                    const Real x3, const Real y3)
{
    return (x1 - x2) * (y2 - y3) - (x2 - x3) * (y1 - y2);
}

} // detail

std::pair<std::size_t, Real>
first_cross_edge(const Barycentric& pos, const Barycentric& disp)
{
    const Barycentric npos = pos + disp;

    if(npos[0] * npos[1] * npos[2] < 0.) // (+, +, -) or one of its permutations
    {
        if     (npos[0] < 0.)
        {return std::make_pair(1, detail::cross_section(pos, disp, 1));}
        else if(npos[1] < 0.)
        {return std::make_pair(2, detail::cross_section(pos, disp, 2));}
        else if(npos[2] < 0.)
        {return std::make_pair(0, detail::cross_section(pos, disp, 0));}
        else {throw std::logic_error("Polygon::crossed_edge: never reach here");}
    }
    else // (+, -, -) or one of its permutations
    {
        if(npos[0] > 0. && npos[1] > 0. && npos[2] > 0) // not (+, +, +) case
            throw std::invalid_argument("BDPolygon::crossed_edge");

        if(npos[0] > 0.)
        {
            const Real ab = detail::cross_section(pos, disp, 0);
            const Real ca = detail::cross_section(pos, disp, 2);
            return (ab > ca) ? std::make_pair(2, ca) : std::make_pair(0, ab);
        }
        else if(npos[1] > 0.)
        {
            const Real ab = detail::cross_section(pos, disp, 0);
            const Real bc = detail::cross_section(pos, disp, 1);
            return (bc > ab) ? std::make_pair(0, ab) : std::make_pair(1, bc);
        }
        else // if(npos[2] > 0.)
        {
            const Real bc = detail::cross_section(pos, disp, 1);
            const Real ca = detail::cross_section(pos, disp, 2);
            return (ca > bc) ? std::make_pair(1, bc) : std::make_pair(2, ca);
        }
    }
}

Barycentric force_put_inside(const Barycentric& bary)
{
    if(!on_plane(bary))
    {
        throw std::invalid_argument("force_put_inside: outside of the plane");
    }
    if(is_inside(bary))
    {
        return bary;
    }

    Barycentric retval(bary);
    boost::array<bool, 3> over;
    over[0] = over[1] = over[2] = false;
    for(std::size_t i=0; i<3; ++i)
    {
        if(retval[i] < 0.)
        {
            over[i]   = true;
            retval[i] = 0;
        }
        else if(retval[i] > 1.)
        {
            over[i]   = true;
            retval[i] = 1.;
        }
    }
    if(!over[0])
    {
        retval[0] = 1. - retval[1] - retval[2];
    }
    else if(!over[1])
    {
        retval[1] = 1. - retval[0] - retval[2];
    }
    else if(!over[2])
    {
        retval[2] = 1. - retval[0] - retval[1];
    }
    else
    {
        throw std::invalid_argument("force_put_inside: too far");
    }
    return retval;
}

Barycentric to_barycentric(const Real3& pos, const Triangle& face)
{
    const Real3& a = face.vertex_at(0);
    const Real3& b = face.vertex_at(1);
    const Real3& c = face.vertex_at(2);
    const Real3  m = cross_product(face.edge_at(0), face.edge_at(2)) * (-1.);
    const Real   x = std::abs(m[0]);
    const Real   y = std::abs(m[1]);
    const Real   z = std::abs(m[2]);

    Real nu, nv, ood;
    if(x >= y && x >= z)
    {
        nu = detail::triangle_area_2D(pos[1], pos[2], b[1], b[2], c[1], c[2]);
        nv = detail::triangle_area_2D(pos[1], pos[2], c[1], c[2], a[1], a[2]);
        ood = 1.0 / m[0];
    }
    else if(y >= x && y >= z)
    {
        nu = detail::triangle_area_2D(pos[0], pos[2], b[0], b[2], c[0], c[2]);
        nv = detail::triangle_area_2D(pos[0], pos[2], c[0], c[2], a[0], a[2]);
        ood = 1.0 / -m[1];
    }
    else
    {
        nu = detail::triangle_area_2D(pos[0], pos[1], b[0], b[1], c[0], c[1]);
        nv = detail::triangle_area_2D(pos[0], pos[1], c[0], c[1], a[0], a[1]);
        ood = 1.0 / m[2];
    }
    Barycentric bary;
    bary[0] = nu * ood;
    bary[1] = nv * ood;
    bary[2] = 1.0 - bary[0] - bary[1];
    return bary;
}

}// ecell4
