#pragma once
#include "pde.hpp"
#include "permutations.hpp"
#include "program_options.hpp"
#include "tensors.hpp"
#include <algorithm>
#include <map>
#include <vector>

namespace elements
{
// yield single-d linear index for level/cell combo
int64_t get_1d_index(int const level, int const cell);

// yield level/cell for a single-d index
std::array<int64_t, 2> get_level_cell(int64_t const single_dim_id);

// return the linear index given element coordinates
template<typename P>
int64_t map_to_index(fk::vector<int> const &coords, options const &opts,
                     PDE<P> const &pde);

// return the element coordinates given linear index
template<typename P>
fk::vector<int>
map_to_coords(int64_t const id, options const &opts, PDE<P> const &pde);

// -----------------------------------------------------------------------------
// element table
// this object's purpose is:
// - to maintain a list of active element IDs
// - to provide a mapping from an assigned index (ordering) of an elements
//   to its coordinates
// - store a flattened version of this for access on devie
//
// about coordinates
// - coordinates are composed of a set of dimension-many pairs (level, cell).
//   (technically, an element's coordinates will also have a degree component,
//   but we omit this as we assume uniform degree).
// - in our table, coordinates are stored with all level components grouped,
//   followed by cell components so that a single coordinate will look like
//   e.g.: (lev_1, lev_2, ..., lev_dim, cell_1, cell_2, ..., cell_dim).
//
// re: full and sparse grid
// - to build a full grid, all potentional level combinations are included in
//   the table; that is, all dimension-length permutations of integers less
//   than or equal to the number of levels selected for the simulation are
//   valid components.
// - to build a sparse grid, we apply some rule to omit some of these
//   permutations. currently, we cull level combinations whose sum is greater
//   than the number of levels selected for the simulation.
// -----------------------------------------------------------------------------

class table
{
public:
  template<typename P>
  table(options const opts, PDE<P> const &pde);

  // get id of element given its 0,...,n index in active elements
  int64_t get_element_id(int64_t const index) const
  {
    assert(index >= 0);
    assert(index < static_cast<int64_t>(active_element_ids_.size()));
    return active_element_ids_[index];
  }

  // lookup coords by index
  fk::vector<int> const &get_coords(int64_t const index) const
  {
    assert(index >= 0);
    assert(index < size());
    return id_to_coords_.at(active_element_ids_[index]);
  }

  // get flattened element table for device
  fk::vector<int, mem_type::owner, resource::device> const &
  get_active_table() const
  {
    return active_table_d_;
  }

  // returns the number of (active) elements in table
  int64_t size() const
  {
    assert(active_element_ids_.size() == id_to_coords_.size());
    return active_element_ids_.size();
  }

  // static construction helper
  // conceptually private, exposed for testing
  // return the cell indices given a level tuple
  static fk::matrix<int> get_cell_index_set(fk::vector<int> const &levels);

private:
  // FIXME change to fk vector if upgraded to 64 bit indexing
  // ordering of active elements
  std::vector<int64_t> active_element_ids_;

  // map from element id to coords
  std::unordered_map<int64_t, fk::vector<int>> id_to_coords_;

  // table of active elements staged for on-device kron list building
  fk::vector<int, mem_type::owner, resource::device> active_table_d_;
};

} // end namespace elements
