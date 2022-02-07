/*------------------------------------------------------------
 * Filename: SQLParser.hpp
 * Author: Joshua Dahl
 * Email: joshuadahl@nevada.unr.edu
 * Created: 2/7/22
 * Modified: 2/7/22
 * Description: Headerfile which provides an interface (single parse function) to the parser.
 *------------------------------------------------------------*/

#ifndef SQL_PARSER_HPP
#define SQL_PARSER_HPP

#include "SQL.hpp"

#include <cmath>
#include <iostream>

// Number struct representing the result of a NumericLiteral
struct Number {
	// Stored integer portion
	int64_t integer;
	// (optional) stored decimal portion
	std::optional<std::string> fraction;
	// (optional) stored exponent portion
	std::optional<int16_t> exponent;

	// Function which checks if this number represents a valid integer
	bool validInteger() {
		if (fraction.has_value()) return false;
		if (exponent.has_value() && exponent.value() < 0) return false;
		return true;
	}

	// Function that returns the number as a (possibly truncated) integer
	int64_t asInt() {
		if(!exponent.has_value())
			return integer;

		return asFloat();
	}

	// Function that returns the number as a floating point type
	double asFloat() {
		double value = integer;

		// Itteratively apply the fraction
		if(fraction.has_value()){
			double ratio = 1.0/10;
			for(char num: fraction.value()){
				value += (num - '0') * ratio;
				ratio /= 10;
			}
		}

		// Apply the exponent
		if(exponent.has_value())
			value *= pow(10, exponent.value());

		return value;
	}
};

// Function which parses a SQL command
sql::ast::Transaction::ptr parseSQL(std::string command);

#endif // SQL_PARSER_HPP