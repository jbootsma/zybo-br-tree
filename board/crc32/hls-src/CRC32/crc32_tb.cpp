#include <cstddef>
#include <cstdint>
#include <cstring>

#include "crc32.hpp"


struct test_vec
{
	const char* src;
	std::size_t len;
	std::uint32_t crc;
};

#define TEST_VEC(str, crc) \
	{ str, sizeof str - 1, crc }

static test_vec tests[] = {
	TEST_VEC("1", 0x83dcefb7),
	TEST_VEC("123456789", 0xCBF43926),
	TEST_VEC("\x00\x00\x00\x00", ~0xdebb20e3),
	TEST_VEC("123456789\x26\x39\xF4\xCB", ~0xdebb20e3),
	TEST_VEC("Hello World!", 0x1C291CA3),
	TEST_VEC("AAAA", 0x9B0D08F1),
};

int main(void)
{
	for (auto& test : tests)
	{
		std::uint32_t crc = ~crc32((const std::uint8_t*)test.src, test.len, ~0);
		if (test.crc != crc)
		{
			return -1;
		}
	}

	return 0;
}
