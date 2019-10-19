#ifndef CRC32_HPP
#define CRC32_HPP

#include <cstdint>

std::uint32_t crc32(const std::uint8_t *mem, std::uint32_t len, std::uint32_t init_val);

#endif
