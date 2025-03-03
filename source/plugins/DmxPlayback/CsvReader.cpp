#include "CsvReader.h"
#include <fstream>
#include <vector>
#include <sstream>
#include <iostream>

// Reads a CSV file into a vector of <uint16_t, vector<uint8_t>> pairs where
// each pair represents <column name, column values>
// Source: https://www.gormanalysis.com/blog/reading-and-writing-csv-files-with-cpp/#reading-from-csv
std::vector< std::pair< std::uint16_t, std::vector< uint8_t > > > read_csv_recording(std::string filename)
{
	// Create a vector of <uint16_t, uint8_t vector> pairs to store the result
	std::vector< std::pair< std::uint16_t, std::vector< uint8_t > > > result;

	// Create an input filestream
	std::ifstream myFile(filename);

	// Make sure the file is open
	if (!myFile.is_open())
	{
		throw std::runtime_error("Could not open file");
	}

	// Helper vars
	std::string line, colname;
	int val;

	// Read the column names
	if (myFile.good())
	{
		// Extract the first line in the file
		std::getline(myFile, line);

		// Create a stringstream from line
		std::stringstream ss(line);

		// Extract each column name
		while (std::getline(ss, colname, ','))
		{
			// Initialize and add <colname, int vector> pairs to result
			result.push_back({ std::stoi(colname), std::vector< uint8_t >{} });
		}
	}

	// Read data, line by line
	while (std::getline(myFile, line))
	{
		// Create a stringstream of the current line
		std::stringstream ss(line);

		// Keep track of the current column index
		int colIdx = 0;

		// Extract each integer
		while (ss >> val)
		{
			// Add the current integer to the 'colIdx' column's values vector
			result.at(colIdx).second.push_back(val);

			// If the next token is a comma, ignore it and move on
			if (ss.peek() == ',')
				ss.ignore();

			// Increment the column index
			colIdx++;
		}
	}

	// Close file
	myFile.close();

	return result;
}
