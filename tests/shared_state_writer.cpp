#include <uuid_map/uuid_map.hpp>

void write_from_another_translation_unit(std::uint32_t actor_0, std::uint32_t actor_1,
                                         std::uint32_t actor_2, std::uint32_t local_index,
                                         std::uint32_t value) {
  sovereignbase::uuid_map::write(actor_0, actor_1, actor_2, local_index, value);
}
