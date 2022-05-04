/*------------------------------------------------------------
 * Filename: main.cpp
 * Author: Joshua Dahl
 * Email: joshuadahl@nevada.unr.edu
 * Created: 2/7/22
 * Modified: 4/13/22
 * Description: Main driver of the program, responsible for collecting user input, executing the parse, and then executing the proper operations.
 *------------------------------------------------------------*/

#include <ctype.h>
#include <ext/alloc_traits.h>
#include <stddef.h>
#include <iostream>
#include <set>
#include <algorithm>
#include <array>
#include <filesystem>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>
#include <thread>

#include "reader.hpp"
#include "SQLparser.hpp"
#include "SQL.hpp"
#include "SimpleBinStream.h"

// Constant representing the filename of database metadata files
constexpr const char* metadataFileName = ".metadata";

// Struct storing the state of the program
struct ProgramState {
	// Directory where our manages databases are stored
	std::filesystem::path databaseDirectory = std::filesystem::current_path();
	// The database we are currently managing (it is optional since at the start of the program we will not be managing a database)
	std::optional<sql::Database> currentDatabase;

	// Pointer to the current transaction, if it is null that means there isn't currently a transaction
	std::unique_ptr<sql::TransactionAction> transaction = nullptr;
};

// Dispatcher function prototypes
void use(const sql::Action& action, ProgramState& state);
void create(const sql::Action& action, ProgramState& state);
void drop(const sql::Action& action, ProgramState& state);
void alter(const sql::Action& action, ProgramState& state);
void insert(const sql::Action& action, ProgramState& state);
void query(const sql::Action& action, ProgramState& state);
void update(const sql::Action& action, ProgramState& state);
void delete_(const sql::Action& action, ProgramState& state);
// Execution function prototypes
void transaction(std::unique_ptr<sql::Action> action, ProgramState& state);
void useDatabase(const sql::Action& action, ProgramState& state, bool quiet = false);
void createDatabase(const sql::Action& action, ProgramState& state);
void createTable(const sql::Action& action, ProgramState& state);
void dropDatabase(const sql::Action& action, ProgramState& state);
void dropTable(const sql::Action& action, ProgramState& state);
void alterTable(const sql::Action& action, ProgramState& state);
void insertIntoTable(const sql::Action& action, ProgramState& state);
void queryTable(const sql::Action& action, ProgramState& state);
void updateTable(const sql::Action& action, ProgramState& state);
void deleteFromTable(const sql::Action& action, ProgramState& state);

// Function which splits a string into a vector of substrings at the specified separators
static std::vector<std::string> split(std::string s, const char* separators = " \t\v\f\r\n", size_t pos = 0, size_t max_splits = -1) {
	size_t start = pos, splits = 0;
	pos = s.find_first_of(separators, pos);
	std::vector<std::string> out;

	// While there are still strings to split
	while (pos != std::string::npos && splits < max_splits) {
		if(pos - start > 0) {
			out.emplace_back(s.substr(start, pos - start));
			splits++;
		}

		start = pos + 1;
		pos = s.find_first_of(separators, start);
	}

	out.emplace_back(s.substr(start, std::string::npos));

	return out;
}

// Function which makes a string lowercase
std::string tolower(std::string s){
	for(char& c: s)
		c = tolower(c);
	return s;
}

// Function which removes all of the deliminating characters from the left side of a string
inline std::string ltrim (const std::string s, const char* delims = " \t\v\f\r\n") {
	if(size_t pos = s.find_first_not_of(delims); pos != std::string::npos)
		return s.substr(pos);
	return ""; // Return a null string if we couldn't find any non-delimiter characters
}
// Function which removes all of the deliminating characters from the right side of a string
inline std::string rtrim (const std::string& s, const char* delims = " \t\v\f\r\n") {
	if(size_t pos = s.find_last_not_of(delims); pos != std::string::npos)
		return s.substr(0, pos + 1);
	return ""; // Return a null string if we couldn't find any non-delimiter characters
}
// Function which removes all of the deliminating characters from the both sides of a string
inline std::string trim (const std::string& s, const char* delims = " \t\v\f\r\n") {
	return rtrim(ltrim(s, delims), delims);
}


// Main function/entry point, runs a read-loop and dispatches to the proper execution function
int main() {
	// Create input reader
	Reader r = Reader(/*enableHistory*/true)
		.setPrompt("sql> ");

	// Input loop
	ProgramState state;
	bool keepRunning = true;
	while(keepRunning){
		// Read some input from the user
		std::string input = trim(r.read(false));
		while(rtrim(input).back() != ';' && tolower(input).find(".exit") == std::string::npos)
			input += "\n" + trim(r.read(false, "^ "));

		// Remove any comments (and newlines) from the input
		auto lines = split(input, "\n");
		input = "";
		for(size_t i = 0; i < lines.size(); i++)
			if(auto trimmed = trim(lines[i]); !(trimmed[0] == '-' && trimmed[1] == '-'))
				input += lines[i] + " ";

		r.appendToHistory(input);

		// Split the input based on semicolons, so each SQL command is parsed seperately
		std::vector<std::string> inputs = split(input, ";");
		for(std::string& input: inputs) {
			// If there is nothing to do, skip this input
			input = trim(input);
			if(input.empty()) continue;
			// Append the semicolon that was removed when we split
			input += ';';

			// Command to exit the program
			if(tolower(input).find(".exit") != std::string::npos){
				keepRunning = false;
			} else {
				sql::Action::ptr action = parseSQL(input);
				// If we failed to parse the provided statement... continue
				if(action == nullptr)
					continue; // Error message provided by parse

				// Hand off the function to the proper dispatcher based on the action this action wishes to perform
				// NOTE: We dereference the pointer we recieved from the parser, its lifetime extends beyond the function utilization and we can still use polymorphism on references
				switch(action->action){
				break; case sql::Action::Use:
					use(*action, state);
				break; case sql::Action::Create:
					create(*action, state);
				break; case sql::Action::Drop:
					drop(*action, state);
				break; case sql::Action::Alter:
					alter(*action, state);
				break; case sql::Action::Insert:
					insert(*action, state);
				break; case sql::Action::Query:
					query(*action, state);
				break; case sql::Action::Update:
					update(*action, state);
				break; case sql::Action::Delete:
					delete_(*action, state);
				break; case sql::Action::Transaction:
					transaction(std::move(action), state);
				// If the action is unsupported, error
				break; default:
					throw std::runtime_error("!Unsupported action: " + sql::Action::ActionNames[action->action]);
				}
			}
		}
	}

	std::cout << "All done." << std::endl;
}

// Function which executes the proper USE function based on the statement target
inline void use(const sql::Action& action, ProgramState& state){
	switch(action.target.type){
	break; case sql::Action::Target::Database:
		useDatabase(action, state);
	// If the action is unsupported for this target, error
	break; default:
		std::cerr << "!Can not USE a " << sql::Action::Target::TypeNames[action.target.type] << "." << std::endl;
	}
}


// Function which executes the proper CREATE function based on the statement target
inline void create(const sql::Action& action, ProgramState& state){
	switch(action.target.type){
	break; case sql::Action::Target::Database:
		createDatabase(action, state);
	break; case sql::Action::Target::Table:
		createTable(action, state);
	// If the action is unsupported for this target, error
	break; default:
		std::cerr << "!Can not CREATE a " << sql::Action::Target::TypeNames[action.target.type] << "." << std::endl;
	}
}

// Function which executes the proper DROP function based on the statement target
inline void drop(const sql::Action& action, ProgramState& state){
	switch(action.target.type){
	break; case sql::Action::Target::Database:
		dropDatabase(action, state);
	break; case sql::Action::Target::Table:
		dropTable(action, state);
	// If the action is unsupported for this target, error
	break; default:
		std::cerr << "!Can not DROP a " << sql::Action::Target::TypeNames[action.target.type] << "." << std::endl;
	}
}

// Function which executes the proper ALTER function based on the statement target
inline void alter(const sql::Action& action, ProgramState& state){
	switch(action.target.type){
	break; case sql::Action::Target::Table:
		alterTable(action, state);
	// If the action is unsupported for this target, error
	break; default:
		std::cerr << "!Can not ALTER a " << sql::Action::Target::TypeNames[action.target.type] << "." << std::endl;
	}
}

// Function which executes the proper QUERY function based on the statement target
inline void insert(const sql::Action& action, ProgramState& state){
	switch(action.target.type){
	break; case sql::Action::Target::Table:
		insertIntoTable(action, state);
	// If the action is unsupported for this target, error
	break; default:
		std::cerr << "!Can not INSERT into a " << sql::Action::Target::TypeNames[action.target.type] << "." << std::endl;
	}
}

// Function which executes the proper QUERY function based on the statement target
inline void query(const sql::Action& action, ProgramState& state){
	switch(action.target.type){
	break; case sql::Action::Target::Table:
		queryTable(action, state);
	// If the action is unsupported for this target, error
	break; default:
		std::cerr << "!Can not SELECT a " << sql::Action::Target::TypeNames[action.target.type] << "." << std::endl;
	}
}

// Function which executes the proper UPDATE function based on the statement target
inline void update(const sql::Action& action, ProgramState& state){
	switch(action.target.type){
	break; case sql::Action::Target::Table:
		updateTable(action, state);
	// If the action is unsupported for this target, error
	break; default:
		std::cerr << "!Can not UPDATE a " << sql::Action::Target::TypeNames[action.target.type] << "." << std::endl;
	}
}

// Function which executes the proper DELETE function based on the statement target
inline void delete_(const sql::Action& action, ProgramState& state){
	switch(action.target.type){
	break; case sql::Action::Target::Table:
		deleteFromTable(action, state);
	// If the action is unsupported for this target, error
	break; default:
		std::cerr << "!Can not DELETE from a " << sql::Action::Target::TypeNames[action.target.type] << "." << std::endl;
	}
}


// --- Helpers ---

// Helper function that checks if a map contains a key
template<typename Key, typename Value>
bool contains(const std::map<Key, Value>& map, const Key& needle) { return map.find(needle) != map.end(); }

// Function that creates a version of the file's path with the current thread ID appended to the filename
std::filesystem::path threadLocalFile(const std::filesystem::path& path) {
	std::stringstream tid; tid << std::this_thread::get_id();
	auto root = path;

	return root.remove_filename() / (tid.str() + "." + path.filename().string());
}

// Helper function that saves a database's metadata
void saveDatabaseMetadataFile(const sql::Database database){
	simple::file_ostream<std::true_type> fout((database.path / metadataFileName).c_str());
	fout << database;
	fout.close();
}

// Helper function that saves a table's metadata and data
void saveTableFile(const sql::Table& table, ProgramState& state){
	// TODO: create lock (or fail if someone else has lock)

	// If we have a transaction, overwrite the path with a temporary one for the transaction
	auto path = table.path;
	if(state.transaction)
		path = state.transaction->tables[table.path] = threadLocalFile(table.path);

	// Save the table to disk
	simple::file_ostream<std::true_type> fout(path.c_str());
	fout << table;
	fout.close();
}

// Helper that loads a table from file (also ensures that exists, both on disk and in the database)
bool loadTable(sql::Table& table, const sql::Database& database, std::string operation, ProgramState& state){
	// Ensure that the table exists in the current database
	if(std::find(database.tables.begin(), database.tables.end(), table.path) == database.tables.end()){
		std::cerr << "!Failed to " << operation << " table " << table.name << " because it doesn't exist." << std::endl;
		return false;
	}

	// TODO: create lock (or fail if someone else has lock)

	// If the transaction has already overriden this table, load data from the temporary path
	auto path = table.path;
	auto pathCache = table.path; // Save the old path to the table, so we don't succesively grow its path
	if(state.transaction && contains(state.transaction->tables, table.path))
		path = state.transaction->tables[table.path];

	// Ensure that the table exists on disk and load it
	if(!exists(table.path)){
		std::cerr << "!Failed to " << operation << " table " << table.name << " because it does not exist." << std::endl;
		return false;
	}
	simple::file_istream<std::true_type> fin(path.c_str());
	try {
		// Load the table
		fin >> table;
		fin.close();
		// Make sure the table's path is the path to the original table
		table.path = pathCache;

		return true;
	} catch(std::runtime_error) {
		std::cerr << "!Failed to " << operation << " table " << table.name << " because it is corupted." << std::endl;
	}

	// If we failed for any reason then close the file and return false
	fin.close();
	return false;
}

// Function that finds the index of a column in a table given its name
size_t findColumn(sql::Table& table, const std::string& columnName){
	for(size_t i = 0; i < table.columns.size(); i++)
		// Check that the columns name matches exactly, or the column name (with table/after) matches
		if(table.columns[i].name == columnName || split(table.columns[i].name, ".").back() == columnName)
			return i;
	return -1;
}

// Helper function that returns a set of indecies representing tuples that satisfy the where conditions in the provided action
std::vector<size_t> applyWhereConditions(sql::Table& table, sql::WhereAction& action, std::string_view operation) {
	// For each condition, find its associated column (and possibly the column its data is held in) and validate its data
	std::vector<size_t> conditionColumns;
	std::vector<size_t> conditionDataColumns;
	for(auto& condition: action.conditions){
		size_t index = findColumn(table, condition.column);
		if(index == -1){
			std::cerr << "!Failed to " << operation << " table " << action.target.name << " because it doesn't contain a condition column named " << condition.column << "." << std::endl;
			return {};
		}
		// Save the column index of this condition
		conditionColumns.push_back(index);

		// If the condition is a column name, find the column associated with the data
		if(condition.value.index() == 5) {
			const std::string& dataColumn = std::get<sql::Column>(condition.value).name;
			size_t dataIndex = findColumn(table, dataColumn);
			if(index == -1){
				std::cerr << "!Failed to " << operation << " table " << action.target.name << " because it doesn't contain a condition data column named " << dataColumn << "." << std::endl;
				return {};
			}

			// If the columns have incompatible data types, error
			if(!table.columns[index].type.compatibleType(table.columns[dataIndex].type)) {
				std::cerr << "!Failed to " << operation << " table " << action.target.name << " because columns `" << condition.column << "` and `" << dataColumn << "` don't have compatible data types and thus can't be compared." << std::endl;
				return {};
			}

			// Mark the column this data's condition comes from
			conditionDataColumns.push_back(dataIndex);

		// Otherwise validate and adjust the condition's value
		} else {
			const sql::Column& column = table.columns[index];
			auto dataValue = sql::ast::extractData(condition.value);
			if(!sql::Data::validateVariant(column, dataValue, /*parserValidation*/ true)){
				std::cerr << "!Failed to " << operation << " table " << action.target.name << " because column " << column.name
					<< " in condition has type " << column.type.to_string() << " but comparision data of type "
					<< sql::Data::variantTypeString(dataValue) << " provided." << std::endl;
				return {};
			}
			sql::Data::applyColumnAdjustments(column, dataValue);
			condition.value = sql::ast::flatten(dataValue);

			// Mark that this condition doesn't have a data column
			conditionDataColumns.push_back(-1);
		}
	}

	// For each tuple...
	std::vector<size_t> selectedTuples;
	for(size_t i = 0; i < table.tuples.size(); i++){
		sql::Tuple& tuple = table.tuples[i];

		// Check how many of the conditions hold true for that tuple
		size_t validations = 0;
		for(size_t i = 0; i < action.conditions.size(); i++){
			const auto& data = tuple[conditionColumns[i]].data;
			auto& condition = action.conditions[i];
			// If the condition's data comes from the table, grab it; otherwise grab the data stored in the condition
			const auto conditionData = (condition.value.index() == 5 ? tuple[conditionDataColumns[i]].data : sql::ast::extractData(condition.value));

			switch (condition.comp){
			break; case sql::WhereAction::equal:
				if(data == conditionData)
					validations++;
			break; case sql::WhereAction::notEqual:
				if(data != conditionData)
					validations++;
			break; case sql::WhereAction::less:
				if(data < conditionData)
					validations++;
			break; case sql::WhereAction::greater:
				if(data > conditionData)
					validations++;
			break; case sql::WhereAction::lessEqual:
				if(data <= conditionData)
					validations++;
			break; case sql::WhereAction::greaterEqual:
				if(data >= conditionData)
					validations++;
			break; default:
				throw std::runtime_error("Unexpected condition");
			}
		}

		// If all of the conditions hold we need to apply changes to this tuple
		if(validations == action.conditions.size())
			selectedTuples.push_back(i);
	}

	return selectedTuples;
}


// --- Execution Functions ---

// Function that manages the current transaction action
void transaction(std::unique_ptr<sql::Action> action, ProgramState& state) {
	// Sanity checked downcast to the special type of action used by this function
	if(action->action != sql::Action::Transaction)
		throw std::runtime_error("A parsing issue has occured! Somehow a non-TransactionAction has arrived in transaction");
	auto transaction = std::unique_ptr<sql::TransactionAction>(reinterpret_cast<sql::TransactionAction*>(action.release()));

	switch(transaction->transactionAction) {
	break; case sql::TransactionAction::Begin: {
		// If there is already a transaction, then we fail to start a new one
		if(state.transaction != nullptr) {
			std::cerr << "!Failed to begin transaction because another transaction has already been started." << std::endl;
			return;
		}

		// Transfer ownership of this transaction to the program state
		state.transaction = std::move(transaction);
		
		std::cout << "Transaction started." << std::endl;
	}
	break; case sql::TransactionAction::Commit: {
		// If there is not already a transaction, then we fail to finish it
		if(state.transaction == nullptr) {
			std::cerr << "!Failed to commit transaction because one has not been started." << std::endl;
			return;
		}

		// Overwrite the tables with the modififed versions from the transaction
		for(auto& [dest, src]: state.transaction->tables) {
			copy(src, dest, std::filesystem::copy_options::overwrite_existing);
			remove(src);
			// TODO: Remove lock
		}

		// We are no longer in a transaction
		state.transaction = nullptr;

		std::cout << "Transaction committed." << std::endl;
	}
	break; case sql::TransactionAction::Abort: {
		// If there is not already a transaction, then we fail to finish it
		if(state.transaction == nullptr) {
			std::cerr << "!Failed to abort transaction because one has not been started." << std::endl;
			return;
		}

		// Discard the modified versions of the tables
		for(auto& [_, modified]: state.transaction->tables) {
			remove(modified);
			// TODO: Remove lock
		}

		// We are no longer in a transaction
		state.transaction = nullptr;

		std::cout << "Transaction aborted." << std::endl;
	}
	break; default:
		throw std::runtime_error("Unexpected transaction action");
	}
}

// Function which performs a database use action (updates the current database in the state)
// NOTE: The success message can be suppressed by passing true to quiet
void useDatabase(const sql::Action& action, ProgramState& state, bool quiet /*= false*/){
	// Initial data for the database
	sql::Database database;
	database.name = action.target.name;
	database.path = absolute(state.databaseDirectory / database.name);

	// If the database directory doesn't already exist, error
	if(!exists(database.path)){
		std::cerr << "!Failed to use database " << database.name << " because it doesn't exist." << std::endl;
		return;
	}

	// If there is currently a transaction, error
	if(state.transaction) {
		std::cerr << "!Failed to use database " << database.name << " because you can't switch databases during a transaction." << std::endl;
		return;
	}

	// Open the database's metadata file (ensuring it exists)
	if(!exists(database.path / metadataFileName)){
		std::cerr << "!Failed to use database " << database.name << " because its metadata doesn't exist." << std::endl;
		return;
	}
	simple::file_istream<std::true_type> fin((database.path / metadataFileName).c_str());
	try {
		// Load the database's metadata file
		fin >> database;

		// Update the current database
		state.currentDatabase = database;

		if(!quiet) std::cout << "Using database " << database.name << "." << std::endl;
	} catch(std::runtime_error) {
		std::cerr << "!Failed to use database " << database.name << " because its metadata is corupted." << std::endl;
	}
	fin.close();
}

// Function which creates a new database in the filesystem
void createDatabase(const sql::Action& action, ProgramState& state){
	// Initial data for the database
	sql::Database database;
	database.name = action.target.name;
	database.path = absolute(state.databaseDirectory / database.name);

	// If the database directory already exists, error
	if(exists(database.path)){
		std::cerr << "!Failed to create database " << database.name << " because it already exists." << std::endl;
		return;
	}

	// Disallow periods
	if(database.name.find(".") != std::string::npos){
		std::cerr << "!Failed to create database " << database.name << " because database names are not allowed to contain a period." << std::endl;
		return;
	}

	// If there is currently a transaction, error
	if(state.transaction) {
		std::cerr << "!Failed to create database " << database.name << " because you can't create databases during a transaction." << std::endl;
		return;
	}

	// Create directorty for the database and save metadata file
	std::filesystem::create_directory(database.path);
	saveDatabaseMetadataFile(database);

	std::cout << "Database " << database.name << " created." << std::endl;

	// If we aren't currently using a database, start using the new database
	if(!state.currentDatabase.has_value())
		useDatabase({sql::Action::Use, {sql::Action::Target::Database, database.name}}, state);
}

// Function which deletes a database from the filesystem
void dropDatabase(const sql::Action& action, ProgramState& state){
	// Initial data for the database
	sql::Database database;
	database.name = action.target.name;
	database.path = absolute(state.databaseDirectory / database.name);

	// If the database directory doesn't already exist, error
	if(!exists(database.path)){
		std::cerr << "!Failed to delete database " << database.name << " because it doesn't exist." << std::endl;
		return;
	}

	// If there is currently a transaction, error
	if(state.transaction) {
		std::cerr << "!Failed to delete database " << database.name << " because you can't delete databases during a transaction." << std::endl;
		return;
	}

	// Make sure the metadata is valid (if not the database doesn't exist)
	std::string usingCache = state.currentDatabase.value_or(sql::Database{}).name;
	useDatabase({sql::Action::Use, {sql::Action::Target::Database, database.name}}, state, /*quiet*/true);
	if(state.currentDatabase.value_or(sql::Database{}).path != database.path){
		std::cerr << "!Failed to delete database " << database.name << " because it doesn't exist." << std::endl;
		return;
	}
	// Make sure we are still using the same database that we were before
	if(!usingCache.empty())
		useDatabase({sql::Action::Use, {sql::Action::Target::Database, usingCache}}, state, /*quiet*/true);
	else state.currentDatabase = {};

	// Remove the database
	std::filesystem::remove_all(database.path);
	// If we are currently using the database, we are now using nothing
	if(database.path == state.currentDatabase.value_or(sql::Database{}).path)
		state.currentDatabase = {};

	std::cout << "Database " << database.name << " deleted." << std::endl;
}

// Function which creates a table, both on disk and in the currently used database's metadata
void createTable(const sql::Action& _action, ProgramState& state){
	// Sanity checked downcast to the special type of action used by this function
	if(_action.action != sql::Action::Create)
		throw std::runtime_error("A parsing issue has occured! Somehow a non-CreateTableAction has arrived in createTable");
	const sql::CreateTableAction& action = *reinterpret_cast<const sql::CreateTableAction*>(&_action);

	// Make sure that a database is currently being used
	if(!state.currentDatabase.has_value()){
		std::cerr << "!Failed to create table " << action.target.name << " because no database is currently being used." << std::endl;
		return;
	}
	sql::Database& database = *state.currentDatabase;

	// Create a table and set its metadata
	sql::Table table;
	table.name = action.target.name;
	table.path = database.path / (table.name + ".table");
	// Ensure that the table doesn't already exist
	if(exists(table.path)){
		std::cerr << "!Failed to create table " << table.name << " because it already exists." << std::endl;
		return;
	}

	// Disallow periods
	if(table.name.find(".") != std::string::npos){
		std::cerr << "!Failed to create table " << table.name << " because table names are not allowed to contain a period." << std::endl;
		return;
	}

	// Ensure that none of the columns share the same name
	std::set<std::string> columnNames;
	bool duplicates = false;
	for(const sql::Column& c: action.columns) {
		if(columnNames.find(c.name) != columnNames.end()) {
			std::cerr << "!Failed to create table " << table.name << " because it has at least two columns named: " << c.name << "." << std::endl;
			duplicates = true;
		}

		columnNames.insert(c.name);
	}
	if(duplicates) return;

	// Set the table's column metadata
	table.columns = action.columns;
	// Add the table to the database's metadata
	database.tables.push_back(table.path);

	// Save the changes to disk
	saveTableFile(table, state);
	saveDatabaseMetadataFile(database);

	std::cout << "Table " << table.name << " created." << std::endl;
}

// Function which deletes a table, both on disk and in the currently used database's metadata
void dropTable(const sql::Action& action, ProgramState& state){
	// Make sure that a database is currently being used
	if(!state.currentDatabase.has_value()){
		std::cerr << "!Failed to remove table " << action.target.name << " because no database is currently being used." << std::endl;
		return;
	}
	sql::Database& database = *state.currentDatabase;

	// Determine the path to the table
	std::filesystem::path tablePath = database.path / (action.target.name + ".table");
	// Ensure that the table doesn't already exist
	if(!exists(tablePath)){
		std::cerr << "!Failed to delete table " << action.target.name << " because it doesn't exist." << std::endl;
		return;
	}

	// If there is currently a transaction, error
	if(state.transaction) {
		std::cerr << "!Failed to delete table " << action.target.name << " because you can't delete tables during a transaction." << std::endl;
		return;
	}

	// Ensure that the table exists in the current database
	auto itterator = std::find(database.tables.begin(), database.tables.end(), tablePath);
	if(itterator == database.tables.end()){
		std::cerr << "!Failed to delete table " << action.target.name << " because it doesn't exist." << std::endl;
		return;
	}
	// Remove the table from the database
	database.tables.erase(itterator);

	// Save the changes to disk
	std::filesystem::remove(tablePath);
	saveDatabaseMetadataFile(database);

	std::cout << "Table " << action.target.name << " deleted." << std::endl;
}

// Function which modifies the metadata of a action
void alterTable(const sql::Action& _action, ProgramState& state){
	// Sanity checked downcast to the special type of action used by this function
	if(_action.action != sql::Action::Alter)
		throw std::runtime_error("A parsing issue has occured! Somehow a non-AlterTableAction has arrived in alterTable");
	const sql::AlterTableAction& action = *reinterpret_cast<const sql::AlterTableAction*>(&_action);

	// Make sure that a database is currently being used
	if(!state.currentDatabase.has_value()){
		std::cerr << "!Failed to alter table " << action.target.name << " because no database is currently being used." << std::endl;
		return;
	}
	sql::Database& database = *state.currentDatabase;

	// Create a table and set its metadata
	sql::Table table;
	table.name = action.target.name;
	table.path = database.path / (table.name + ".table");

	// Load the table from disk (helper handles ensuring that it exists)
	if(!loadTable(table, database, "alter", state))
		return;

	// Find the index of the target column
	size_t index = -1;
	for(int i = 0; i < table.columns.size(); i++)
		if(table.columns[i].name == action.alterTarget.name) {
			index = i;
			break;
		}

	// Determine how to procede based on the secondary alter action
	switch(action.alterAction){
	break; case sql::Action::Add: {
		// Make sure sure that the column isn't in the metadata, error if present
		if(index != -1){
			std::cerr << "!Failed to add " << action.alterTarget.name << " because it already exists in " << table.name << "." << std::endl;
			return;
		}

		// Disallow periods
		if(action.alterTarget.name.find(".") != std::string::npos){
			std::cerr << "!Failed to add " << action.alterTarget.name << " because column names are not allowed to contain a period." << std::endl;
			return;
		}

		// Add the new column and add null data to represent it
		table.columns.push_back(action.alterTarget);
		for(sql::Tuple& tuple: table.tuples)
			tuple.emplace_back(sql::Data::null(&table.columns.back()));

		std::cout << "Table " << table.name << " modified, added " << action.alterTarget.name << "." << std::endl;
	}
	break; case sql::Action::Remove: {
		// Find the column's index in the table, error if not present
		if(index == -1){
			std::cerr << "!Failed to remove " << action.alterTarget.name << " because it doesn't exist in " << table.name << "." << std::endl;
			return;
		}

		// Remove the column from the metadata and from each tuple
		table.columns.erase(table.columns.begin() + index);
		for(sql::Tuple& tuple: table.tuples)
			tuple.erase(tuple.begin() + index);

		std::cout << "Table " << table.name << " modified, removed " << action.alterTarget.name << "." << std::endl;
	}
	break; case sql::Action::Alter: {
		// Find the column's index in the table, error if not present
		if(index == -1){
			std::cerr << "!Failed to modify " << action.alterTarget.name << " because it doesn't exist in " << table.name << "." << std::endl;
			return;
		}

		// Update the target column and nullify all of the data in that column
		table.columns[index] = action.alterTarget;
		for(sql::Tuple& tuple: table.tuples)
			tuple[index] = sql::Data::null(&table.columns[index]);

		std::cout << "Table " << table.name << " modified, modified " << action.alterTarget.name << "." << std::endl;
	}
	// If the action is unsupported, error
	break; default:
		throw std::runtime_error("!Unsupported action: " + sql::Action::ActionNames[action.alterAction]);
	}

	// Save changes to disk
	saveTableFile(table, state);
}

// Function which inserts a new tuple into a table
void insertIntoTable(const sql::Action& _action, ProgramState& state){
	// Sanity checked downcast to the special type of action used by this function
	if(_action.action != sql::Action::Insert)
		throw std::runtime_error("A parsing issue has occured! Somehow a non-InsertIntoTableAction has arrived in insertIntoTable");
	const sql::InsertIntoTableAction& action = *reinterpret_cast<const sql::InsertIntoTableAction*>(&_action);

	// Make sure that a database is currently being used
	if(!state.currentDatabase.has_value()){
		std::cerr << "!Failed to insert into table " << action.target.name << " because no database is currently being used." << std::endl;
		return;
	}
	sql::Database& database = *state.currentDatabase;

	// Create a temporary table and set its metadata
	sql::Table table;
	table.name = action.target.name;
	table.path = database.path / (table.name + ".table");

	// Load the table from disk (helper handles ensuring that it exists)
	if(!loadTable(table, database, "insert into", state))
		return;

	// Create a new empty tuple in the table
	sql::Tuple& tuple = table.createEmptyTuple();
	// Ensure that the user didn't provide more data than the table can hold (less is fine)
	if(action.values.size() > tuple.size()){
		std::cerr << "!Failed to insert into table " << action.target.name << " expected no more than " << tuple.size()
			<< " pieces of data but " << action.values.size() << " recieved." << std::endl;
		return;
	}

	bool valid = true;
	// For each piece of data the user provided...
	for(size_t i = 0; i < action.values.size(); i++) {
		// Ensure that the data the user provided is of the correct type
		if(!sql::Data::validateVariant(table.columns[i], action.values[i], /*parserValidation*/ true)){
			std::cerr << "!Failed to insert into table " << action.target.name << " because column " << table.columns[i].name
				<< " has type " << table.columns[i].type.to_string() << " but data of type "
				<< sql::Data::variantTypeString(action.values[i]) << " provided." << std::endl;
			valid = false;
			continue;
		}

		// If the data is of the correct type copy it into the table
		tuple[i].data = action.values[i];
	}
	// We are done if any of the data was of the incorrect type
	if(!valid) return;

	// Apply any nessicary adjustments to make the data valid
	for(sql::Data& data: tuple)
		data.applyColumnAdjustments();

	std::cout << "1 new record inserted." << std::endl;

	// Save changes to disk
	saveTableFile(table, state);
}

// Function which performs a query on the data in a table
void queryTable(const sql::Action& _action, ProgramState& state){
	// Sanity checked downcast to the special type of action used by this function
	if(_action.action != sql::Action::Query)
		throw std::runtime_error("A parsing issue has occured! Somehow a non-QueryTableAction has arrived in queryTable");
	sql::QueryTableAction& action = const_cast<sql::QueryTableAction&>(*reinterpret_cast<const sql::QueryTableAction*>(&_action));

	// Make sure that a database is currently being used
	if(!state.currentDatabase.has_value()){
		std::cerr << "!Failed to query table " << action.target.name << " because no database is currently being used." << std::endl;
		return;
	}
	sql::Database& database = *state.currentDatabase;


	// Ensure that none of the Tables share the same alias
	auto aliasCopy = action.tableAliases;
	std::sort(aliasCopy.begin(), aliasCopy.end(), [](const auto& a, const auto& b){ return a.alias < b.alias; });
	auto uniqueEnd = std::unique(aliasCopy.begin(), aliasCopy.end(), [](const auto& a, const auto& b){ return a.alias == b.alias; });
	if(uniqueEnd != aliasCopy.end()){
		std::cerr << "!Failed to preform query becuase it contains multiple tables mapped to the same alias." << std::endl;
		return;
	}


	// Create a temporary table
	sql::Table table;
	// A null bit of state, used so that queries always load from disk instead of the current transaction
	ProgramState nullState;

	// Load all of the tables from disk, cartesian producting them together as nessicary
	for(size_t i = 0; i < action.tableAliases.size(); i++) {
		auto& alias = action.tableAliases[i];
		// Load the table from disk (helper handles ensuring that it exists)
		sql::Table tempTable;
		tempTable.name = alias.table;
		tempTable.path = database.path / (tempTable.name + ".table");
		if(!loadTable(tempTable, database, "query", nullState))
			return;
		// Add the alias to the table columns' names
		for(auto& column: tempTable.columns)
			column.name = alias.alias + "." + column.name;
		// Prepend the index of the data to this new table
		tempTable.columns.insert(tempTable.columns.begin(), {&tempTable, "__index" + std::to_string(i) + "__", {sql::DataType::INT}});
		for(size_t i = 0; i < tempTable.tuples.size(); i++)
			tempTable.tuples[i].insert(tempTable.tuples[i].begin(), {(int64_t)i, &tempTable.columns.front()});


		// Create a new table with all of the columns of both the old and newly loaded tables
		sql::Table cartesianProduct;
		for(auto& column: table.columns)
			cartesianProduct.columns.push_back(column);
		for(auto& column: tempTable.columns)
			cartesianProduct.columns.push_back(column);

		// Preform a cartesian product of the tuples in both tables
		if(table.tuples.empty() && !tempTable.tuples.empty()) {
			table = std::move(tempTable);
			continue;
		}
		if(!table.tuples.empty() && tempTable.tuples.empty())
			continue;
		for(auto& oldTuple: table.tuples)
			for(auto& newTuple: tempTable.tuples) {
				auto& tuple = cartesianProduct.createEmptyTuple();
				for(size_t i = 0; i < oldTuple.size(); i++)
					tuple[i] = oldTuple[i];
				for(size_t i = 0, offset = oldTuple.size(); i < newTuple.size(); i++)
					tuple[i + offset] = newTuple[i];
			}

		// Add extra left and right tuples with opposite null values if we are preforming an outer join
		if(alias.isOuterJoin()) {
			// Add all of the left table tuples with null right values
			for(auto& oldTuple: table.tuples) {
				auto& leftTuple = cartesianProduct.createEmptyTuple();
				for(size_t i = 0; i < oldTuple.size(); i++)
					leftTuple[i] = oldTuple[i];
			}
			// Add all of the right table tuples with null left values
			for(auto& newTuple: tempTable.tuples) {
				auto& rightTuple = cartesianProduct.createEmptyTuple();
				for(size_t i = 0, offset = table.columns.size(); i < newTuple.size(); i++)
					rightTuple[i + offset] = newTuple[i];
			}
		}

		// Move the calculated cartesian product to our table
		table = std::move(cartesianProduct);
	}


	// Select tuples
	if(!action.conditions.empty()){
		// Filter out all of the tuples that don't satisfy the conditions
		auto selectedTuples = applyWhereConditions(table, action, "query");
		if(selectedTuples.empty())
			return;

		// Add in missing left tuples if we are doing a left outer join
		if(action.tableAliases.size() > 1 && action.tableAliases[1].isOuterJoin()){
			// Determine the indecies we have selected
			std::vector<size_t> indeciesFound;
			for(size_t selected: selectedTuples)
				if(!table.tuples[selected][0].isNull())
					indeciesFound.push_back(std::get<int64_t>(table.tuples[selected][0].data)); // Tuple index 0, since we prepended data to the table
			// Determine where the index column for the second table is stored
			size_t rightIndex = findColumn(table, "__index1__");

			// Find all tuples with an index we haven't yet discovered, and a null second half, add them to the table
			for(size_t i = 0; i < table.tuples.size(); i++){
				auto& tuple = table.tuples[i];
				auto dataIndex = tuple[0].isNull() ? -1 : std::get<int64_t>(tuple[0].data);
				if(std::find(indeciesFound.begin(), indeciesFound.end(), dataIndex) == indeciesFound.end()
					&& tuple[rightIndex].isNull())
				{
					selectedTuples.push_back(i);
					indeciesFound.push_back(dataIndex);
				}
			}
		}

		// Move the selected tuples into a new array
		std::vector<sql::Tuple> tuples;
		for(size_t i: selectedTuples)
			tuples.emplace_back(std::move(table.tuples[i]));

		// The array of selected tuples becomes the table's list of tuples
		table.tuples = std::move(tuples);
	}

	// Project tuples (if we aren't selecting all of them)
	if(!action.columns.all()){
		// Calculate the indecies of the tuples we need to keep in the projection
		std::vector<size_t> columnsToKeep;
		for(std::string column: *action.columns){
			size_t index = findColumn(table, column);
			if(index == -1){
				std::cerr << "!Failed to query table " << table.name << " because projection column " << column << " doesn't exist." << std::endl;
				return;
			}

			columnsToKeep.push_back(index);
		}

		// Copy the columns we should keep into a new temporary table
		sql::Table projectedTable;
		for(size_t i: columnsToKeep)
			projectedTable.columns.emplace_back(table.columns[i]);
		for(sql::Tuple& tuple: table.tuples){
			sql::Tuple& projectedTuple = projectedTable.createEmptyTuple();
			size_t i = 0;
			for(size_t keep: columnsToKeep)
				projectedTuple[i++].data = tuple[keep].data;
		}

		// Replace the table with the new projection
		table = std::move(projectedTable);

	// If we are keeping all of the columns, remove the __index#__ columns we added
	} else {
		for(size_t i = 0; i < table.columns.size(); i++)
			if(table.columns[i].name.find("__index") != std::string::npos) {
				table.columns.erase(table.columns.begin() + i);
				for(auto& tuple: table.tuples)
					tuple.erase(tuple.begin() + i);
				i--;
			}
	}

	// If the table has no metadata then there is nothing to display
	if(table.columns.empty())
		return;

	// If there is an active transaction, warn that the show data is outdated
	if(state.transaction)
		std::cout << "NOTE: There is an active transaction, commit the transaction to see its data!" << std::endl;

	// Print out the headers
	std::cout << split(table.columns[0].name, ".").back() << " " << table.columns[0].type.to_string();
	for(int i = 1; i < table.columns.size(); i++)
		std::cout << " | " << split(table.columns[i].name, ".").back() << " " << table.columns[i].type.to_string();
	std::cout << std::endl;

	// Print out the data
	for(sql::Tuple& t: table.tuples){
		bool first = true;
		for(sql::Data& d: t) {
			std::visit([first](auto v){
				if(!first) std::cout << " | ";

				if constexpr(std::is_same_v<decltype(v), std::monostate>) std::cout << "null";
				else std::cout << v;
			}, d.data);
			first = false;
		}
		std::cout << std::endl;
	}
}

// Function which updates the data in a table
void updateTable(const sql::Action& _action, ProgramState& state){
	// Sanity checked downcast to the special type of action used by this function
	if(_action.action != sql::Action::Update)
		throw std::runtime_error("A parsing issue has occured! Somehow a non-UpdateTableAction has arrived in updateTable");
	sql::UpdateTableAction& action = const_cast<sql::UpdateTableAction&>(*reinterpret_cast<const sql::UpdateTableAction*>(&_action));

	// Make sure that a database is currently being used
	if(!state.currentDatabase.has_value()){
		std::cerr << "!Failed to update table " << action.target.name << " because no database is currently being used." << std::endl;
		return;
	}
	sql::Database& database = *state.currentDatabase;

	// Create a temporary table and set its metadata
	sql::Table table;
	table.name = action.target.name;
	table.path = database.path / (table.name + ".table");

	// Load the table from disk (helper handles ensuring that it exists)
	if(!loadTable(table, database, "update", state))
		return;

	// Find the column index that we are updating (error if it doesn't exist)
	size_t columnIndex = findColumn(table, action.column);
	if(columnIndex == -1){
		std::cerr << "!Failed to update table " << action.target.name << " because it doesn't contain a column named " << action.column << "." << std::endl;
		return;
	}

	// Filter out all of the tuples that don't satisfy the conditions
	auto selectedTuples = applyWhereConditions(table, action, "update");
	if(selectedTuples.empty())
		return;

	// Update the value in tuples where all of the conditions hold
	for(size_t tupleIndex: selectedTuples)
		table.tuples[tupleIndex][columnIndex].data = action.value;

	std::cout << selectedTuples.size() << " record" << (selectedTuples.size() > 1 ? "s" : "") << " modified." << std::endl;

	// Save changes to disk
	saveTableFile(table, state);
}

// Function which deletes some data from a table
void deleteFromTable(const sql::Action& _action, ProgramState& state){
	// Sanity checked downcast to the special type of action used by this function
	if(_action.action != sql::Action::Delete)
		throw std::runtime_error("A parsing issue has occured! Somehow a non-DeleteFromTableAction has arrived in updateTable");
	sql::DeleteFromTableAction& action = const_cast<sql::DeleteFromTableAction&>(*reinterpret_cast<const sql::DeleteFromTableAction*>(&_action));

	// Make sure that a database is currently being used
	if(!state.currentDatabase.has_value()){
		std::cerr << "!Failed to update table " << action.target.name << " because no database is currently being used." << std::endl;
		return;
	}
	sql::Database& database = *state.currentDatabase;

	// Create a temporary table and set its metadata
	sql::Table table;
	table.name = action.target.name;
	table.path = database.path / (table.name + ".table");

	// Load the table from disk (helper handles ensuring that it exists)
	if(!loadTable(table, database, "delete from", state))
		return;

	// Filter out all of the tuples that don't satisfy the conditions
	auto selectedTuples = applyWhereConditions(table, action, "delete from");
	if(selectedTuples.empty())
		return;

	// Remove all of the selected tuples from the table
	size_t selectedSize = selectedTuples.size();
	for(size_t i = 0; i < selectedSize; i++){
		table.tuples.erase(table.tuples.begin() + selectedTuples[0]);
		selectedTuples.erase(selectedTuples.begin());

		for(size_t& index: selectedTuples)
			index--;
	}

	std::cout << selectedSize << " record" << (selectedSize > 1 ? "s" : "") << " deleted." << std::endl;

	// Save changes to disk
	saveTableFile(table, state);
}