/*
crc32_test - Test program for example crc32 kernel module.
Copyright (C) 2019  James Bootsma <jrbootsma@gmail.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include <cstdint>
#include <fstream>
#include <iostream>

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

std::uint32_t crc32(const char * src, std::size_t len)
{
	// Hard-coded for now. In realife this would probably be determined
	// by looking under the class dir in /sys/
	std::fstream fh(
		"/dev/crc",
		std::ios::in|std::ios::out|std::ios::binary
	);

	fh.write(src, len);
	fh.flush();

	std::uint32_t res;
	fh.read((char*)&res, sizeof res);

	return res;
}

int main(void)
{
	for(auto& test : tests)
	{
		std::cout << "Checking CRC for " << test.src << std::endl;
		std::cout << "Expect " << std::hex << test.crc << std::endl;
		std::cout << "Got " << std::hex <<
			crc32(test.src, test.len) << std::endl;
	}
}
