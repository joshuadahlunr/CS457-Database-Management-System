#ifndef SQL_HPP
#define SQL_HPP

#include <string>
#include <filesystem>
#include <vector>
#include <map>
#include <optional>

namespace sql {

	// Forward declarations
	struct Database;
	struct Table;
	struct Record;

	template<typename T>
	struct Wildcard: public std::optional<T>{
		using std::optional<T>::optional;

		// Query if the wildcard (everything) was selected
		bool all() { return !this->has_value(); }
		// Implicitly convert to the stored value
		operator T() { return this->value(); } // TODO: Remove?
		operator T&() { return this->value(); }
	};

	// Struct representing a DataType
	struct DataType {
		enum Type {
			Invalid,
			INT,
			FLOAT,
			CHAR,
			VARCHAR,
			TEXT,
		};

		// Type code
		Type type;
		// String size (used to indicate the length of char and varchar types)
		uint16_t size = 1;
	};

	// Struct representing a column in a table
	struct Column {
		// Pointer to the table this column belongs to
		Table* table;

		// Name of the column
		std::string name;
		// Type of data stored in this column
		DataType type;

		// Modified constructors to offload some work from the SQL parser
		Column(Table* table = nullptr, std::string name = "", DataType type = {}): table(table), name(name), type(type) {}
		Column(std::string name, DataType type = {}): table(nullptr), name(name), type(type) {}
	};


	// Struct representing one piece of data stored in a column
	struct Data {
		// Pointer to the column this piece of data belongs to
		Column* column;
		// Pointer to the record (row) this piece of data belongs to
		Record* record;

		// Bool which indicates if there is data stored here or not
		bool null = false;
		// Union actually storing the data (the column's data type indicates which member is currently in use)
		union {
			int64_t _int;
			double _float;
			std::string _string;
		};
	};

	// Struct representing a row in the table (this class is a thing wrapper around std::vector)
	struct Record: public std::vector<Data> {
		// Pointer to the table this record belongs to
		Table* table;

		using std::vector<Data>::vector;
	};

	// Struct representing a table
	struct Table {
		// Pointer to the database this table belongs to
		Database* database;

		// The name of this table
		std::string name;
		// The columns of this table
		std::vector<Column> columns;

		// A pointer to the records this table is storing
		std::unique_ptr<std::vector<Record>> records;
	};

	// Struct representing a database
	struct Database {
		// The name of this database
		std::string name;

		// The filesystem path to this database
		std::filesystem::path path;
		// Relative filesystem paths to the tables this database manages
		std::vector<std::filesystem::path> tables;

		std::map<std::string, Table*> tableMap; // TODO: should this map, map paths to tables?
	};


	namespace ast {
		// Struct which represents a single command provided by the user
		struct Transaction {
			// Smart pointer wrapper around a transaction
			using ptr = std::unique_ptr<ast::Transaction>;

			// Types of Transactions (with different signatures)
			enum Type {
				Base,
				QueryTable,
				CreateTable,
				AlterTable,
			};

			// Actions a transaction can be performing
			enum Action {
				Invalid,
				Use,
				Create,
				Drop,
				Alter,
				Select,

				// Actions specific to table alters // TODO: Should we consildate these into add and drop?
				Add,
				Remove,
			};

			// Struct which represents the target of this command
			struct Target {
				enum Type {
					Invalid,
					Database,
					Table,
					Column,
				};

				// Type of target
				Type type;
				// Name of the target
				std::string name;
			};

			// The type of transaction (used to determine how to downcast)
			Type type;
			// The action to be taken by this command
			Action action;
			// The target of this command
			Target target;
		};

		// Struct representing a table transaction
		struct QueryTableTransaction: public Transaction {
			// The columns (or wildcard) to query
			Wildcard<std::vector<std::string>> columns;
		};

		// Struct representing a table creation transaction
		struct CreateTableTransaction: public Transaction {
			// The column metadata to create the table with
			std::vector<Column> columns;
		};

		// Struct representing a table alteration transaction
		struct AlterTableTransaction: public Transaction {
			// The action to be taken on a column of the table
			Action alterAction;
			// The column of the table to be altered
			Column alterTarget;  // Remove only uses the name, ignoring the datatype
		};

	} // ast


} // sql

#endif // SQL_HPP