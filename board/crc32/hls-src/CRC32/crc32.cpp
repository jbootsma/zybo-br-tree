#include <cstdint>

#include "crc32.hpp"

std::uint32_t crc32(const std::uint8_t *mem, std::uint32_t len, std::uint32_t init_val)
{
#pragma HLS INTERFACE s_axilite port=init_val
#pragma HLS INTERFACE s_axilite port=len
#pragma HLS INTERFACE s_axilite port=return
#pragma HLS INTERFACE m_axi depth=16 port=mem offset=slave

	static const std::uint32_t poly = 0xEDB88320;

	std::uint32_t reg = init_val;

	while (len--)
	{
		std::uint8_t data = *(mem++);

		for (size_t i = 0; i < 8; i++)
		{
			bool next_bit = !!(data & 0x01) ^ !!(reg & 0x01);

			data >>= 1;
			reg >>= 1;
			if (next_bit)
			{
				reg ^= poly;
			}
		}
	}

	return reg;
}
