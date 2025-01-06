#include "whippch.h"
#include "filesystem.h"

_WHIP_START

raw_buffer filesystem::read_file_binary(const std::filesystem::path& filepath)
{
	std::ifstream stream(filepath, std::ios::binary | std::ios::ate);
	if (!stream)
		return {};

	std::streampos end = stream.tellg();
	stream.seekg(0, std::ios::beg);
	uint64_t size = end - stream.tellg();

	if (size == 0)
		return {};

	raw_buffer buffer(size);
	stream.read(buffer.as<char>(), size);
	stream.close();
	return buffer;
}

_WHIP_END
