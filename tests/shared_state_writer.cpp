#include <uuid_map/uuid_map.hpp>

void write_from_another_translation_unit(std::uint64_t actor_hi, std::uint32_t actor_lo,
                                         std::uint32_t local_index, std::uint32_t value) {
  sovereignbase::uuid_map::write(actor_hi, actor_lo, local_index, value);
}
