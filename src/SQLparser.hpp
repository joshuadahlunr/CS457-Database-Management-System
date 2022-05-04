/*------------------------------------------------------------
 * Filename: SQLParser.hpp
 * Author: Joshua Dahl
 * Email: joshuadahl@nevada.unr.edu
 * Created: 2/7/22
 * Modified: 5/4/22
 * Description: Headerfile which provides an interface (single parse function) to the parser.
 *------------------------------------------------------------*/

#ifndef SQL_PARSER_HPP
#define SQL_PARSER_HPP

#include "SQL.hpp"

#include <cmath>
#include <iostream>

// Function which parses a SQL command
sql::ast::Action::ptr parseSQL(std::string command);

#endif // SQL_PARSER_HPP