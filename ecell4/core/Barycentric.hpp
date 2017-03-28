#ifndef ECELL_BARYCENTRIC
#define ECELL_BARYCENTRIC
#include <ecell4/core/Triangle.hpp>

namespace ecell4
{

template<typename realT>
struct Barycentric
{
    typedef realT       real_type;
    typedef real_type   value_type;
    typedef std::size_t size_type;
    typedef size_type   index_type;
    typedef boost::array<real_type, 3> container_type;

    Barycentric(){}
    ~Barycentric(){}

    Barycentric(const real_type a, const real_type b, const real_type c);
    Barycentric(const Barycentric& b);
    Barycentric& operator=(const Barycentric& b);

    real_type  operator[](const index_type i) const throw() {return val[i];}
    real_type& operator[](const index_type i)       throw() {return val[i];}
    real_type  at(const index_type i) const {return val.at(i);}
    real_type& at(const index_type i)       {return val.at(i);}

  private:
    boost::array<real_type, 3> val;
};

template<typename realT>
inline Barycentric<realT>::Barycentric(
        const real_type a, const real_type b, const real_type c)
{
    val[0] = a;
    val[1] = b;
    val[2] = c;
}

template<typename realT>
inline Barycentric<realT>::Barycentric(const Barycentric<realT>& b)
{
    val[0] = b.val[0];
    val[1] = b.val[1];
    val[2] = b.val[2];
}

template<typename realT>
inline Barycentric<realT>& Barycentric<realT>::operator=(
        const Barycentric<realT>& b)
{
    val[0] = b.val[0];
    val[1] = b.val[1];
    val[2] = b.val[2];
    return *this;
}

template<typename realT>
inline Barycentric<realT>
operator+(const Barycentric<realT>& lhs, const Barycentric<realT>& rhs)
{
    return Barycentric<realT>(lhs[0]+rhs[0], lhs[1]+rhs[1], lhs[2]+rhs[2]);
}


template<typename realT>
inline Barycentric<realT>
operator-(const Barycentric<realT>& lhs, const Barycentric<realT>& rhs)
{
    return Barycentric<realT>(lhs[0]-rhs[0], lhs[1]-rhs[1], lhs[2]-rhs[2]);
}

template<typename realT>
inline bool on_plane(
        const Barycentric<realT>& bary, const realT tolerance = 1e-10)
{
    return std::abs(bary[0] + bary[1] + bary[2] - 1.0) < tolerance;
}

template<typename realT>
inline bool is_inside(const Barycentric<realT>& bary)
{
    return on_plane(bary) &&
           (0. <= bary[0] && bary[0] <= 1.0) &&
           (0. <= bary[1] && bary[1] <= 1.0) &&
           (0. <= bary[2] && bary[2] <= 1.0);
}

template<typename realT>
inline bool is_inside(const Barycentric<realT>& bary, const realT tolerance)
{
    return on_plane(bary) &&
           (0.0 - tolerance <= bary[0] && bary[0] <= 1.0 + tolerance) &&
           (0.0 - tolerance <= bary[1] && bary[1] <= 1.0 + tolerance) &&
           (0.0 - tolerance <= bary[2] && bary[2] <= 1.0 + tolerance);
}

template<typename realT>
inline Real cross_section(const Barycentric<realT>& pos,
                          const Barycentric<realT>& displacement,
                          const std::size_t edge_idx)
{
    const std::size_t idx = (edge_idx==0) ? 2 : edge_idx-1;
    return -pos[idx] / displacement[idx];
}

template<typename realT>
std::pair<std::size_t, realT>
first_cross_edge(const Barycentric<realT>& pos,
                 const Barycentric<realT>& disp)
{
    const Barycentric<realT> npos = pos + disp;

    if(npos[0] * npos[1] * npos[2] < 0.) // (+, +, -) or one of its permutations
    {
             if(npos[0] < 0.) return std::make_pair(1, cross_section(pos, disp, 1));
        else if(npos[1] < 0.) return std::make_pair(2, cross_section(pos, disp, 2));
        else if(npos[2] < 0.) return std::make_pair(0, cross_section(pos, disp, 0));
        else throw std::logic_error("Polygon::crossed_edge: never reach here");
    }
    else // (+, -, -) or one of its permutations
    {
        if(npos[0] > 0. && npos[1] > 0. && npos[2] > 0) // not (+, +, +) case
            throw std::invalid_argument("BDPolygon::crossed_edge");

        if(npos[0] > 0.)
        {
            const realT ab = cross_section(pos, disp, 0);
            const realT ca = cross_section(pos, disp, 2);
            return (ab > ca) ? std::make_pair(2, ca) : std::make_pair(0, ab);
        }
        else if(npos[1] > 0.)
        {
            const realT ab = cross_section(pos, disp, 0);
            const realT bc = cross_section(pos, disp, 1);
            return (bc > ab) ? std::make_pair(0, ab) : std::make_pair(1, bc);
        }
        else // if(npos[2] > 0.)
        {
            const realT bc = cross_section(pos, disp, 1);
            const realT ca = cross_section(pos, disp, 2);
            return (ca > bc) ? std::make_pair(1, bc) : std::make_pair(2, ca);
        }
    }
}

template<typename realT>
Barycentric<realT> force_put_inside(const Barycentric<realT>& bary)
{
    if(!on_plane(bary))
        throw std::invalid_argument("force_put_inside: outside of the plane");
    if(is_inside(bary)) return bary;

    Barycentric<realT> retval(bary);
    boost::array<bool, 3> over;
    over[0] = over[1] = over[2] = false;
    for(std::size_t i=0; i<3; ++i)
    {
        if(retval[i] < 0.)
        {
            over[i] = true;
            retval[i] = 0;
        }
        else if(retval[i] > 1.)
        {
            over[i] = true;
            retval[i] = 1.;
        }
    }
    if(!over[0])
        retval[0] = 1. - retval[1] - retval[2];
    else if(!over[1])
        retval[1] = 1. - retval[0] - retval[2];
    else if(!over[2])
        retval[2] = 1. - retval[0] - retval[1];
    else
        throw std::invalid_argument("force_put_inside: too far");

    return retval;
}

inline Real3 to_absolute(const Barycentric<Real>& bary, const Triangle& tri)
{
    return tri.vertex_at(0) * bary[0] +
           tri.vertex_at(1) * bary[1] +
           tri.vertex_at(2) * bary[2];
}

namespace detail
{

template<typename realT>
inline realT triangle_area_2D(const realT x1, const realT y1,
        const realT x2, const realT y2, const realT x3, const realT y3)
{
    return (x1 - x2) * (y2 - y3) - (x2 - x3) * (y1 - y2);
}

} // detail

Barycentric<Real>
to_barycentric(const Real3& pos, const Triangle& face);

template<typename realT, typename charT, typename traitsT>
inline std::basic_ostream<charT, traitsT>&
operator<<(std::basic_ostream<charT, traitsT>& os, const Barycentric<realT>& b)
{
    os << b[0] << ", " << b[1] << ", " << b[2];
    return os;
}

}// ecell4
#endif // ECELL_BARYCENTRIC
