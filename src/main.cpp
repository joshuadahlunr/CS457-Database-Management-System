#include <iostream>
#include "reader.hpp"

#include "SQLparser.hpp"

// Struct storing the state of the program
struct ProgramState {
	// Directory where our manages databases are stored
	std::filesystem::path databaseDirectory = std::filesystem::current_path();
	// The database we are currently managing (it is optional since at the start of the program we will not be managing a database)
	std::optional<sql::Database> currentDatabase;
};

// Dispatcher function prototypes
void use(const sql::ast::Transaction& transaction, ProgramState& state);
void create(const sql::ast::Transaction& transaction, ProgramState& state);
void drop(const sql::ast::Transaction& transaction, ProgramState& state);
void alter(const sql::ast::Transaction& transaction, ProgramState& state);
void query(const sql::ast::Transaction& transaction, ProgramState& state);
// Execution function prototypes
void useDatabase(const sql::ast::Transaction& transaction, ProgramState& state);
void createDatabase(const sql::ast::Transaction& transaction, ProgramState& state);
void createTable(const sql::ast::Transaction& transaction, ProgramState& state);
void dropDatabase(const sql::ast::Transaction& transaction, ProgramState& state);
void dropTable(const sql::ast::Transaction& transaction, ProgramState& state);
void alterTable(const sql::ast::Transaction& transaction, ProgramState& state);
void queryTable(const sql::ast::Transaction& transaction, ProgramState& state);

// Function which makes a string lowercase
std::string tolower(std::string s){
	for(char& c: s)
		c = tolower(c);
	return s;
}


int main() {
	std::cout << "Hello World!" << std::endl;

	Reader r = Reader(/*enableHistory*/true)
		.setPrompt("> ");

	// Input loop 
	ProgramState state;
	bool keepRunning = true;
	while(keepRunning){
		std::string input = r.read();

		// Command to exit the program
		if(tolower(input) == ".exit"){
			keepRunning = false;
		} else {
			sql::ast::Transaction::ptr transaction = parseSQL(input);
			// If we failed to parse the provided statement... continue
			if(transaction == nullptr)
				continue; // Error message provided by parse

			// Hand off the function to the proper dispatcher based on the action this transaction wishes to perform
			// NOTE: We dereference the pointer we recieved from the parser, its lifetime extends beyond the function utilization and we can still use polymorphism on references
			switch(transaction->action){
			break; case sql::ast::Transaction::Use:
				use(*transaction, state);
			break; case sql::ast::Transaction::Create:
				create(*transaction, state);
			break; case sql::ast::Transaction::Drop:
				drop(*transaction, state);
			break; case sql::ast::Transaction::Alter:
				alter(*transaction, state);
			break; case sql::ast::Transaction::Query:
				query(*transaction, state);
			// If the action is unsupported, error
			break; default:
				std::cout << "!Unsupported action: " << sql::ast::Transaction::ActionNames[transaction->action] << std::endl;
			}
		}
	}

	std::cout << "All done" << std::endl;
}

// Function which executes the proper USE function based on the statement target
inline void use(const sql::ast::Transaction& transaction, ProgramState& state){
	switch(transaction.target.type){
	break; case sql::ast::Transaction::Target::Database:
		useDatabase(transaction, state);
	// If the action is unsupported for this target, error
	break; default:
		std::cerr << "!Can not USE a " << sql::ast::Transaction::Target::TypeNames[transaction.target.type] << "." << std::endl;
	}
}


// Function which executes the proper CREATE function based on the statement target
inline void create(const sql::ast::Transaction& transaction, ProgramState& state){
	switch(transaction.target.type){
	break; case sql::ast::Transaction::Target::Database:
		createDatabase(transaction, state);
	break; case sql::ast::Transaction::Target::Table:
		createTable(transaction, state);
	// If the action is unsupported for this target, error
	break; default:
		std::cerr << "!Can not CREATE a " << sql::ast::Transaction::Target::TypeNames[transaction.target.type] << "." << std::endl;
	}
}

// Function which executes the proper DROP function based on the statement target
inline void drop(const sql::ast::Transaction& transaction, ProgramState& state){
	switch(transaction.target.type){
	break; case sql::ast::Transaction::Target::Database:
		dropDatabase(transaction, state);
	break; case sql::ast::Transaction::Target::Table:
		dropTable(transaction, state);
	// If the action is unsupported for this target, error
	break; default:
		std::cerr << "!Can not DROP a " << sql::ast::Transaction::Target::TypeNames[transaction.target.type] << "." << std::endl;
	}
}

// Function which executes the proper ALTER function based on the statement target
inline void alter(const sql::ast::Transaction& transaction, ProgramState& state){
	switch(transaction.target.type){
	break; case sql::ast::Transaction::Target::Table:
		alterTable(transaction, state);
	// If the action is unsupported for this target, error
	break; default:
		std::cerr << "!Can not ALTER a " << sql::ast::Transaction::Target::TypeNames[transaction.target.type] << "." << std::endl;
	}
}

// Function which executes the proper QUERY function based on the statement target
inline void query(const sql::ast::Transaction& transaction, ProgramState& state){
	switch(transaction.target.type){
	break; case sql::ast::Transaction::Target::Table:
		queryTable(transaction, state);
	// If the action is unsupported for this target, error
	break; default:
		std::cerr << "!Can not SELECT a " << sql::ast::Transaction::Target::TypeNames[transaction.target.type] << "." << std::endl;
	}
}


// -- Execution Functions --


void useDatabase(const sql::ast::Transaction& transaction, ProgramState& state){
	std::cout << "u TODO!" << std::endl;
}

void createDatabase(const sql::ast::Transaction& transaction, ProgramState& state){
	std::cout << "cd TODO!" << std::endl;
}

void createTable(const sql::ast::Transaction& _transaction, ProgramState& state){
	// Sanity checked downcast to the special type of transaction used by this function
	if(_transaction.type != sql::ast::Transaction::CreateTable)
		throw std::runtime_error("A parsing issue has occured! Somehow a non-CreateTableTransaction has arrived in createTable");
	const sql::ast::CreateTableTransaction& transaction = *reinterpret_cast<const sql::ast::CreateTableTransaction*>(&_transaction);
	std::cout << "ct TODO!" << std::endl;
}

void dropDatabase(const sql::ast::Transaction& transaction, ProgramState& state){
	std::cout << "dd TODO!" << std::endl;
}

void dropTable(const sql::ast::Transaction& transaction, ProgramState& state){
	std::cout << "dt TODO!" << std::endl;
}

void alterTable(const sql::ast::Transaction& _transaction, ProgramState& state){
	// Sanity checked downcast to the special type of transaction used by this function
	if(_transaction.type != sql::ast::Transaction::AlterTable)
		throw std::runtime_error("A parsing issue has occured! Somehow a non-AlterTableTransaction has arrived in alterTable");
	const sql::ast::AlterTableTransaction& transaction = *reinterpret_cast<const sql::ast::AlterTableTransaction*>(&_transaction);
	std::cout << "a TODO!" << std::endl;
}

void queryTable(const sql::ast::Transaction& _transaction, ProgramState& state){
	// Sanity checked downcast to the special type of transaction used by this function
	if(_transaction.type != sql::ast::Transaction::QueryTable)
		throw std::runtime_error("A parsing issue has occured! Somehow a non-QueryTableTransaction has arrived in queryTable");
	const sql::ast::QueryTableTransaction& transaction = *reinterpret_cast<const sql::ast::QueryTableTransaction*>(&_transaction);
	std::cout << "q TODO!" << std::endl;
}