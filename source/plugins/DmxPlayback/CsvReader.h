#pragma once
#include <vector>
#include <sstream>

std::vector< std::pair< std::uint16_t, std::vector< uint8_t > > > read_csv_recording( std::string filename );
