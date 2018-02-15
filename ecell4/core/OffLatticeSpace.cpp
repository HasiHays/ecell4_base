#include <algorithm>
#include "Context.hpp"
#include "VacantType.hpp"
#include "MolecularType.hpp"
#include "OffLatticeSpace.hpp"

namespace ecell4 {

OffLatticeSpace::OffLatticeSpace(const Real& voxel_radius)
    : base_type(voxel_radius),
      voxels_(),
      positions_(),
      adjoinings_()
{}

OffLatticeSpace::OffLatticeSpace(const Real& voxel_radius,
                                 const position_container& positions,
                                 const coordinate_pair_list_type& adjoining_pairs)
    : base_type(voxel_radius),
      voxels_(),
      positions_(),
      adjoinings_()
{
    reset(positions, adjoining_pairs);
}

OffLatticeSpace::~OffLatticeSpace() {}

void OffLatticeSpace::reset(const position_container& positions,
                            const coordinate_pair_list_type& adjoining_pairs)
{
    voxels_.clear();
    positions_.clear();
    adjoinings_.clear();

    const std::size_t size(positions.size());

    voxels_.resize(size, vacant_.get());
    positions_.resize(size);
    adjoinings_.resize(size);

    std::copy(positions.begin(), positions.end(), positions_.begin());

    for (coordinate_pair_list_type::const_iterator itr(adjoining_pairs.begin());
         itr != adjoining_pairs.end(); ++itr)
    {
        const coordinate_type coord0((*itr).first);
        const coordinate_type coord1((*itr).second);

        if (is_in_range(coord0) && is_in_range(coord1))
        {
            adjoinings_.at(coord0).push_back(coord1);
            adjoinings_.at(coord1).push_back(coord0);
        }
        else
        {
            throw IllegalState("A given pair is invalid.");
        }
    }
}

VoxelPool* OffLatticeSpace::get_voxel_pool(const Voxel& v)
{
    const Species& sp(v.species());

    {
        voxel_pool_map_type::iterator itr(voxel_pools_.find(sp));
        if (itr != voxel_pools_.end())
        {
            return (*itr).second.get();
        }
    }

    {
        molecule_pool_map_type::iterator itr(molecule_pools_.find(sp));
        if (itr != molecule_pools_.end())
        {
            return (*itr).second.get();  // upcast
        }
    }

    // Create a new molecular pool

    const bool suc = make_molecular_pool(sp, v.radius(), v.D(), v.loc());
    if (!suc)
    {
        throw IllegalState("never reach here");
    }

    molecule_pool_map_type::iterator i = molecule_pools_.find(sp);
    if (i == molecule_pools_.end())
    {
        throw IllegalState("never reach here");
    }
    return (*i).second.get();  // upcast
}

boost::optional<OffLatticeSpace::coordinate_type>
OffLatticeSpace::get_coord(const ParticleID& pid) const
{
    if (pid == ParticleID())
        return boost::none;

    for (molecule_pool_map_type::const_iterator itr(molecule_pools_.begin());
         itr != molecule_pools_.end(); ++itr)
    {
        const boost::shared_ptr<MoleculePool>& vp((*itr).second);
        for (MoleculePool::const_iterator vitr(vp->begin());
             vitr != vp->end(); ++vitr)
        {
            if ((*vitr).pid == pid)
            {
                return (*vitr).coordinate;
            }
        }
    }
    // throw NotFound("A corresponding particle is not found");
    return boost::none;
}

bool OffLatticeSpace::make_molecular_pool(
        const Species& sp, Real radius, Real D, const std::string loc)
{
    molecule_pool_map_type::iterator itr(molecule_pools_.find(sp));
    if (itr != molecule_pools_.end())
    {
        return false;
    }
    else if (voxel_pools_.find(sp) != voxel_pools_.end())
    {
        throw IllegalState(
            "The given species is already assigned to the VoxelPool with no voxels.");
    }

    boost::shared_ptr<VoxelPool> location;
    if (loc == "")
    {
        location = vacant_;
    }
    else
    {
        const Species locsp(loc);
        try
        {
            location = find_voxel_pool(locsp);
        }
        catch (const NotFound& err)
        {
            // XXX: A VoxelPool for the structure (location) must be allocated
            // XXX: before the allocation of a Species on the structure.
            // XXX: The VoxelPool cannot be automatically allocated at the time
            // XXX: because its MoleculeInfo is unknown.
            // XXX: LatticeSpaceVectorImpl::load will raise a problem about this issue.
            // XXX: In this implementation, the VoxelPool for a structure is
            // XXX: created with default arguments.
            boost::shared_ptr<MoleculePool>
                locmt(new MolecularType(locsp, vacant_, voxel_radius_, 0));
            std::pair<molecule_pool_map_type::iterator, bool>
                locval(molecule_pools_.insert(
                    molecule_pool_map_type::value_type(locsp, locmt)));
            if (!locval.second)
            {
                throw AlreadyExists(
                    "never reach here. find_voxel_pool seems wrong.");
            }
            location = (*locval.first).second;
        }
    }

    boost::shared_ptr<MoleculePool>
        vp(new MolecularType(sp, location, radius, D));
    std::pair<molecule_pool_map_type::iterator, bool>
        retval(molecule_pools_.insert(
            molecule_pool_map_type::value_type(sp, vp)));
    if (!retval.second)
    {
        throw AlreadyExists("never reach here.");
    }
    return retval.second;
}


/*
 * public functions
 */

// Same as LatticeSpaceVectorImpl
std::pair<ParticleID, Voxel>
OffLatticeSpace::get_voxel_at(const coordinate_type& coord) const
{
    const VoxelPool* vp(voxels_.at(coord));
    return std::make_pair(vp->get_particle_id(coord),
                          Voxel(vp->species(),
                                coord,
                                vp->radius(),
                                vp->D(),
                                get_location_serial(vp)));
}

// Same as LatticeSpaceVectorImpl
const Particle OffLatticeSpace::particle_at(const coordinate_type& coord) const
{
    const VoxelPool* vp(voxels_.at(coord));
    return Particle(
        vp->species(),
        coordinate2position(coord),
        vp->radius(), vp->D());
}

// Same as LatticeSpaceVectorImpl
bool OffLatticeSpace::update_voxel(const ParticleID& pid, const Voxel& v)
{
    const coordinate_type& to_coord(v.coordinate());
    if (!is_in_range(to_coord))
        throw NotSupported("Out of bounds");

    VoxelPool* new_vp(get_voxel_pool(v)); //XXX: need MoleculeInfo
    VoxelPool* dest_vp(get_voxel_pool_at(to_coord));

    if (dest_vp != new_vp->location().get()) // XXX: remove .get()
    {
        throw NotSupported(
            "Mismatch in the location. Failed to place '"
            + new_vp->species().serial() + "' to '"
            + dest_vp->species().serial() + "'.");
    }

    if (boost::optional<coordinate_type> from_coord = get_coord(pid))
    {
        // move
        VoxelPool* src_vp(voxels_.at(*from_coord));
        src_vp->remove_voxel_if_exists(*from_coord);

        //XXX: use location?
        dest_vp->replace_voxel(to_coord, *from_coord);
        voxel_container::iterator from_itr(voxels_.begin() + *from_coord);
        (*from_itr) = dest_vp;

        new_vp->add_voxel(coordinate_id_pair_type(pid, to_coord));
        voxel_container::iterator to_itr(voxels_.begin() + to_coord);
        (*to_itr) = new_vp;
        return false;
    }

    // new
    dest_vp->remove_voxel_if_exists(to_coord);

    new_vp->add_voxel(coordinate_id_pair_type(pid, to_coord));
    voxel_container::iterator to_itr(voxels_.begin() + to_coord);
    (*to_itr) = new_vp;
    return true;
}
// Same as LatticeSpaceVectorImpl
bool OffLatticeSpace::remove_voxel(const ParticleID& pid)
{
    for (molecule_pool_map_type::iterator i(molecule_pools_.begin());
         i != molecule_pools_.end(); ++i)
    {
        const boost::shared_ptr<MoleculePool>& vp((*i).second);
        MoleculePool::const_iterator j(vp->find(pid));
        if (j != vp->end())
        {
            const coordinate_type coord((*j).coordinate);
            if (!vp->remove_voxel_if_exists(coord))
            {
                return false;
            }

            voxel_container::iterator itr(voxels_.begin() + coord);
            (*itr) = vp->location().get(); // XXX: remove .get()
            vp->location()->add_voxel(
                coordinate_id_pair_type(ParticleID(), coord));
            return true;
        }
    }
    return false;
}

// Same as LatticeSpaceVectorImpl
bool OffLatticeSpace::remove_voxel(const coordinate_type& coord)
{
    voxel_container::iterator itr(voxels_.begin() + coord);
    VoxelPool* vp(*itr);
    if (vp->is_vacant())
    {
        return false;
    }
    if (vp->remove_voxel_if_exists(coord))
    {
        (*itr) = vp->location().get(); // XXX: remove .get()
        vp->location()->add_voxel(
            coordinate_id_pair_type(ParticleID(), coord));
        return true;
    }
    return false;
}

bool OffLatticeSpace::can_move(const coordinate_type& src, const coordinate_type& dest) const
{
    if (src == dest) return false;

    const VoxelPool* src_vp(voxels_.at(src));
    if (src_vp->is_vacant()) return false;

    VoxelPool* dest_vp(voxels_.at(dest));

    return (voxels_.at(dest) == src_vp->location().get()); // XXX: remove .get()
}

bool OffLatticeSpace::move(const coordinate_type& src, const coordinate_type& dest,
        const std::size_t candidate)
{
    if (src == dest) return false;

    VoxelPool* src_vp(voxels_.at(src));
    if (src_vp->is_vacant()) return true;

    VoxelPool* dest_vp(voxels_.at(dest));
    if (dest_vp != src_vp->location().get()) return false; // XXX: remove .get()

    src_vp->replace_voxel(src, dest, candidate);
    voxel_container::iterator src_itr(voxels_.begin() + src);
    (*src_itr) = dest_vp;

    dest_vp->replace_voxel(dest, src);
    voxel_container::iterator dest_itr(voxels_.begin() + dest);
    (*dest_itr) = src_vp;

    return true;
}

std::pair<OffLatticeSpace::coordinate_type, bool>
OffLatticeSpace::move_to_neighbor(
        VoxelPool* const& src_vp, VoxelPool* const& loc,
        coordinate_id_pair_type& info, const Integer nrand)
{
    const coordinate_type src(info.coordinate);
    coordinate_type dest(get_neighbor(src, nrand));

    VoxelPool* dest_vp(voxels_.at(dest));

    if (dest_vp != loc)
        return std::make_pair(dest, false);

    voxels_.at(src) = loc; // == dest_vp
    voxels_.at(dest) = src_vp;

    src_vp->replace_voxel(src, dest);
    dest_vp->replace_voxel(dest, src);
    return std::make_pair(dest, true);
}

OffLatticeSpace::coordinate_type
OffLatticeSpace::position2coordinate(const Real3& pos) const
{
    coordinate_type coordinate(0);
    Real shortest_length = length(positions_.at(0) - pos);

    for (coordinate_type coord(1); coord < size(); ++coord)
    {
        const Real len(length(positions_.at(coord) - pos));
        if (len < shortest_length)
        {
            coordinate = coord;
            shortest_length = len;
        }
    }

    return coordinate;
}

} // ecell4
