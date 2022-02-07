#include <iostream>
#include "reader.hpp"
#include "SQLparser.hpp"

// Constant representing the filename of database metadata files
inline constexpr const char* metadataFile = ".metadata";

// Struct storing the state of the program
struct ProgramState {
	// Directory where our manages databases are stored
	std::filesystem::path databaseDirectory = std::filesystem::current_path();
	// The database we are currently managing (it is optional since at the start of the program we will not be managing a database)
	std::optional<sql::Database> currentDatabase;
};

// Dispatcher function prototypes
void use(const sql::Transaction& transaction, ProgramState& state);
void create(const sql::Transaction& transaction, ProgramState& state);
void drop(const sql::Transaction& transaction, ProgramState& state);
void alter(const sql::Transaction& transaction, ProgramState& state);
void query(const sql::Transaction& transaction, ProgramState& state);
// Execution function prototypes
void useDatabase(const sql::Transaction& transaction, ProgramState& state, bool quiet = false);
void createDatabase(const sql::Transaction& transaction, ProgramState& state);
void createTable(const sql::Transaction& transaction, ProgramState& state);
void dropDatabase(const sql::Transaction& transaction, ProgramState& state);
void dropTable(const sql::Transaction& transaction, ProgramState& state);
void alterTable(const sql::Transaction& transaction, ProgramState& state);
void queryTable(const sql::Transaction& transaction, ProgramState& state);

// Function which makes a string lowercase
std::string tolower(std::string s){
	for(char& c: s)
		c = tolower(c);
	return s;
}


int main() {
	// Create input reader
	Reader r = Reader(/*enableHistory*/true)
		.setPrompt("> ");

	// Input loop 
	ProgramState state;
	bool keepRunning = true;
	while(keepRunning){
		// Read some input from the user
		std::string input = r.read();

		// Command to exit the program
		if(input.starts_with("--")){
			// Skip comments
		} else if(tolower(input) == ".exit"){
			keepRunning = false;
		} else {
			sql::Transaction::ptr transaction = parseSQL(input);
			// If we failed to parse the provided statement... continue
			if(transaction == nullptr)
				continue; // Error message provided by parse

			// Hand off the function to the proper dispatcher based on the action this transaction wishes to perform
			// NOTE: We dereference the pointer we recieved from the parser, its lifetime extends beyond the function utilization and we can still use polymorphism on references
			switch(transaction->action){
			break; case sql::Transaction::Use:
				use(*transaction, state);
			break; case sql::Transaction::Create:
				create(*transaction, state);
			break; case sql::Transaction::Drop:
				drop(*transaction, state);
			break; case sql::Transaction::Alter:
				alter(*transaction, state);
			break; case sql::Transaction::Query:
				query(*transaction, state);
			// If the action is unsupported, error
			break; default:
				throw std::runtime_error("!Unsupported action: " + sql::Transaction::ActionNames[transaction->action]);
			}
		}
	}

	std::cout << "All done" << std::endl;
}

// Function which executes the proper USE function based on the statement target
inline void use(const sql::Transaction& transaction, ProgramState& state){
	switch(transaction.target.type){
	break; case sql::Transaction::Target::Database:
		useDatabase(transaction, state);
	// If the action is unsupported for this target, error
	break; default:
		std::cerr << "!Can not USE a " << sql::Transaction::Target::TypeNames[transaction.target.type] << "." << std::endl;
	}
}


// Function which executes the proper CREATE function based on the statement target
inline void create(const sql::Transaction& transaction, ProgramState& state){
	switch(transaction.target.type){
	break; case sql::Transaction::Target::Database:
		createDatabase(transaction, state);
	break; case sql::Transaction::Target::Table:
		createTable(transaction, state);
	// If the action is unsupported for this target, error
	break; default:
		std::cerr << "!Can not CREATE a " << sql::Transaction::Target::TypeNames[transaction.target.type] << "." << std::endl;
	}
}

// Function which executes the proper DROP function based on the statement target
inline void drop(const sql::Transaction& transaction, ProgramState& state){
	switch(transaction.target.type){
	break; case sql::Transaction::Target::Database:
		dropDatabase(transaction, state);
	break; case sql::Transaction::Target::Table:
		dropTable(transaction, state);
	// If the action is unsupported for this target, error
	break; default:
		std::cerr << "!Can not DROP a " << sql::Transaction::Target::TypeNames[transaction.target.type] << "." << std::endl;
	}
}

// Function which executes the proper ALTER function based on the statement target
inline void alter(const sql::Transaction& transaction, ProgramState& state){
	switch(transaction.target.type){
	break; case sql::Transaction::Target::Table:
		alterTable(transaction, state);
	// If the action is unsupported for this target, error
	break; default:
		std::cerr << "!Can not ALTER a " << sql::Transaction::Target::TypeNames[transaction.target.type] << "." << std::endl;
	}
}

// Function which executes the proper QUERY function based on the statement target
inline void query(const sql::Transaction& transaction, ProgramState& state){
	switch(transaction.target.type){
	break; case sql::Transaction::Target::Table:
		queryTable(transaction, state);
	// If the action is unsupported for this target, error
	break; default:
		std::cerr << "!Can not SELECT a " << sql::Transaction::Target::TypeNames[transaction.target.type] << "." << std::endl;
	}
}


// --- Helpers ---


// Helper function that saves a database's metadata
void saveDatabaseMetadataFile(const sql::Database database){
	simple::file_ostream<std::true_type> fout((database.path / metadataFile).c_str());
	fout << database;
	fout.close();
}

// Helper function that saves a table's metadata and data
void saveTableFile(const sql::Table table){
	simple::file_ostream<std::true_type> fout(table.path.c_str());
	fout << table;
	fout.close();
}


// --- Execution Functions ---


// Function which performs a database use transaction (updates the current database in the state)
// NOTE: The success message can be suppressed by passing true to quiet
void useDatabase(const sql::Transaction& transaction, ProgramState& state, bool quiet /*= false*/){
	// Initial data for the database
	sql::Database database;
	database.name = transaction.target.name;
	database.path = absolute(state.databaseDirectory / database.name);

	// If the database directory doesn't already exist, error
	if(!exists(database.path)){
		std::cerr << "!Failed to use database " << database.name << " because it doesn't exist." << std::endl;
		return;
	}

	// Open the database's metadata file (ensuring it exists)
	if(!exists(database.path / metadataFile)){
		std::cerr << "!Failed to use database " << database.name << " because its metadata doesn't exist." << std::endl;
		return;
	}
	simple::file_istream<std::true_type> fin((database.path / metadataFile).c_str());
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
void createDatabase(const sql::Transaction& transaction, ProgramState& state){
	// Initial data for the database
	sql::Database database;
	database.name = transaction.target.name;
	database.path = absolute(state.databaseDirectory / database.name);

	// If the database directory already exists, error
	if(exists(database.path)){
		std::cerr << "!Failed to create database " << database.name << " because it already exists." << std::endl;
		return;
	}
	
	// Create directorty for the database and save metadata file
	std::filesystem::create_directory(database.path);
	saveDatabaseMetadataFile(database);

	std::cout << "Database " << database.name << " created." << std::endl;

	// If we aren't currently using a database, start using the new database
	if(!state.currentDatabase.has_value())
		useDatabase({sql::Transaction::Base, sql::Transaction::Use, {sql::Transaction::Target::Database, database.name}}, state);
}

// Function which deletes a database from the filesystem
void dropDatabase(const sql::Transaction& transaction, ProgramState& state){
	// Initial data for the database
	sql::Database database;
	database.name = transaction.target.name;
	database.path = absolute(state.databaseDirectory / database.name);

	// If the database directory doesn't already exist, error
	if(!exists(database.path)){
		std::cerr << "!Failed to dtbl_1elete database " << database.name << " because it doesn't exist." << std::endl;
		return;
	}

	// Make sure the metadata is valid (if not the database doesn't exist)
	std::string usingCache = state.currentDatabase.value_or(sql::Database{}).name;
	useDatabase({sql::Transaction::Base, sql::Transaction::Use, {sql::Transaction::Target::Database, database.name}}, state, /*quiet*/true);
	if(state.currentDatabase.value_or(sql::Database{}).path != database.path){
		std::cerr << "!Failed to delete database " << database.name << " because it doesn't exist." << std::endl;
		return;
	}
	// Make sure we are still using the same database that we were before
	if(!usingCache.empty())
		useDatabase({sql::Transaction::Base, sql::Transaction::Use, {sql::Transaction::Target::Database, usingCache}}, state, /*quiet*/true);
	else state.currentDatabase = {};

	// Remove the database
	std::filesystem::remove_all(database.path);
	// If we are currently using the database, we are now using nothing
	if(database.path == state.currentDatabase.value_or(sql::Database{}).path)
		state.currentDatabase = {};

	std::cout << "Database " << database.name << " deleted." << std::endl;
}

void createTable(const sql::Transaction& _transaction, ProgramState& state){
	// Sanity checked downcast to the special type of transaction used by this function
	if(_transaction.type != sql::Transaction::CreateTable)
		throw std::runtime_error("A parsing issue has occured! Somehow a non-CreateTableTransaction has arrived in createTable");
	const sql::CreateTableTransaction& transaction = *reinterpret_cast<const sql::CreateTableTransaction*>(&_transaction);

	// Make sure that a database is currently being used
	if(!state.currentDatabase.has_value()){
		std::cerr << "!Failed to create table " << transaction.target.name << " because no database is currently being used." << std::endl;
		return;
	}
	sql::Database& database = *state.currentDatabase;

	// Create a table and set its metadata
	sql::Table table;
	table.name = transaction.target.name;
	table.path = database.path / (table.name + ".table");
	// Ensure that the table doesn't already exist
	if(exists(table.path)){
		std::cerr << "!Failed to create table " << table.name << " because it already exists." << std::endl;
		return;
	}
	// Set the table's column metadata
	table.columns = transaction.columns;
	// Add the table to the database's metadata
	database.tables.push_back(table.path);

	// Save the changes to disk
	saveTableFile(table);
	saveDatabaseMetadataFile(database);

	std::cout << "Table " << table.name << " created." << std::endl;
}

void dropTable(const sql::Transaction& transaction, ProgramState& state){
	// Make sure that a database is currently being used
	if(!state.currentDatabase.has_value()){
		std::cerr << "!Failed to remove table " << transaction.target.name << " because no database is currently being used." << std::endl;
		return;
	}
	sql::Database& database = *state.currentDatabase;
	
	// Determine the path to the table
	std::filesystem::path tablePath = database.path / (transaction.target.name + ".table");
	// Ensure that the table doesn't already exist
	if(!exists(tablePath)){
		std::cerr << "!Failed to delete table " << transaction.target.name << " because it doesn't exist." << std::endl;
		return;
	}

	// Ensure that the table exists in the current database
	auto itterator = std::find(database.tables.begin(), database.tables.end(), tablePath);
	if(itterator == database.tables.end()){
		std::cerr << "!Failed to delete table " << transaction.target.name << " because it doesn't exist." << std::endl;
		return;
	}
	// Remove the table from the database
	database.tables.erase(itterator);

	// Save the changes to disk
	std::filesystem::remove(tablePath);
	saveDatabaseMetadataFile(database);

	std::cout << "Table " << transaction.target.name << " deleted." << std::endl;
}

void alterTable(const sql::Transaction& _transaction, ProgramState& state){
	// Sanity checked downcast to the special type of transaction used by this function
	if(_transaction.type != sql::Transaction::AlterTable)
		throw std::runtime_error("A parsing issue has occured! Somehow a non-AlterTableTransaction has arrived in alterTable");
	const sql::AlterTableTransaction& transaction = *reinterpret_cast<const sql::AlterTableTransaction*>(&_transaction);

	// Make sure that a database is currently being used
	if(!state.currentDatabase.has_value()){
		std::cerr << "!Failed to alter table " << transaction.target.name << " because no database is currently being used." << std::endl;
		return;
	}
	sql::Database& database = *state.currentDatabase;
	std::cout << "a TODO!" << std::endl;
}

void queryTable(const sql::Transaction& _transaction, ProgramState& state){
	// Sanity checked downcast to the special type of transaction used by this function
	if(_transaction.type != sql::Transaction::QueryTable)
		throw std::runtime_error("A parsing issue has occured! Somehow a non-QueryTableTransaction has arrived in queryTable");
	const sql::QueryTableTransaction& transaction = *reinterpret_cast<const sql::QueryTableTransaction*>(&_transaction);

	// Make sure that a database is currently being used
	if(!state.currentDatabase.has_value()){
		std::cerr << "!Failed to query table " << transaction.target.name << " because no database is currently being used." << std::endl;
		return;
	}
	sql::Database& database = *state.currentDatabase;
	
	// Create a table and set its metadata
	sql::Table table;
	table.name = transaction.target.name;
	table.path = database.path / (table.name + ".table");
	
	// Ensure that the table exists and load it
	if(!exists(table.path)){
		std::cerr << "!Failed to query table " << table.name << " because it does not exist." << std::endl;
		return;
	}
	simple::file_istream<std::true_type> fin(table.path.c_str());
	try {
		// Load the table
		fin >> table;

		// If the table has no data then there is nothing to display
		if(table.columns.empty())
			return;

		// Print out the headers
		std::cout << table.columns[0].name << " " << table.columns[0].type.to_string();
		for(int i = 1; i < table.columns.size(); i++)
			std::cout << " | " << table.columns[i].name << " " << table.columns[i].type.to_string();
		std::cout << std::endl;

		// TODO: Implement query logic
	} catch(std::runtime_error) {
		std::cerr << "!Failed to query tavle " << database.name << " because it is corupted." << std::endl;
	}
}