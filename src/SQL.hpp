/*------------------------------------------------------------
 * Filename: SQL.hpp
 * Author: Joshua Dahl
 * Email: joshuadahl@nevada.unr.edu
 * Created: 2/7/22
 * Modified: 2/7/22
 * Description: Provides several data structs which hold database, tables, columns, records, etc...,
 * 				also provides transactions the the parser creates as well as serialization for these things.
 *------------------------------------------------------------*/

#ifndef SQL_HPP
#define SQL_HPP

#include <array>
#include <filesystem>
#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>
#include <SimpleBinStream.h>

// std::filesystem::path Serialization
namespace std::filesystem {
	template<typename same_endian_type> typename simple::file_ostream<same_endian_type>& operator<< ( simple::file_ostream<same_endian_type>& s, const std::filesystem::path& p) { return s << p.string(); }
	template<typename same_endian_type> typename simple::file_istream<same_endian_type>& operator>> ( simple::file_istream<same_endian_type>& s, std::filesystem::path& p) {
		std::string temp;
		s >> temp;
		p = std::filesystem::path{temp};
		return s;
	}

	template<typename same_endian_type> typename simple::file_ostream<same_endian_type>& operator<< ( simple::file_ostream<same_endian_type>& s, const std::vector<std::filesystem::path>& v) {
		s << v.size();
		for(auto &p: v)
			s << p;
		return s;
	}
	template<typename same_endian_type> typename simple::file_istream<same_endian_type>& operator>> ( simple::file_istream<same_endian_type>& s, std::vector<std::filesystem::path>& v) {
		size_t size;
		s >> size;
		v.resize(size);
		for(size_t i = 0; i < size; i++)
			s >> v[i];
		return s;
	}
}


namespace sql {

	// Forward declarations
	struct Database;
	struct Table;
	struct Record;

	// Wrapper around std::optional that provides support for replacing values with a wildcard
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

		// Function which converts a datatype to a string
		std::string to_string() const {
			switch(type){
			break; case INT:
				return "int";
			break; case FLOAT:
				return "float";
			break; case CHAR:
				return "char(" + std::to_string(size) + ")";
			break; case VARCHAR:
				return "varchar(" + std::to_string(size) + ")";
			break; case TEXT:
				return "text";
			default:
				return "unknown-type";
			}
		}
	};
	// Datatype De/serialization
	template<typename same_endian_type> typename simple::file_ostream<same_endian_type>& operator << ( simple::file_ostream<same_endian_type>& s, const DataType& d) {
		return s << d.type << d.size;
	}
	template<typename same_endian_type> typename simple::file_istream<same_endian_type>& operator >> ( simple::file_istream<same_endian_type>& s, DataType& d) {
		return s >> d.type >> d.size;
	}

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
	// Column De/serialization
	template<typename same_endian_type> typename simple::file_ostream<same_endian_type>& operator << ( simple::file_ostream<same_endian_type>& s, const Column& c) {
		return s << c.name << c.type;
	}
	template<typename same_endian_type> typename simple::file_istream<same_endian_type>& operator >> ( simple::file_istream<same_endian_type>& s, Column& c) {
		return s >> c.name >> c.type;
	}
	// Vector of column De/serialization
	template<typename same_endian_type> typename simple::file_ostream<same_endian_type>& operator << ( simple::file_ostream<same_endian_type>& s, const std::vector<Column>& v) {
		s << v.size();
		for(const Column& c: v)
			s << c;
		return s;
	}
	template<typename same_endian_type> typename simple::file_istream<same_endian_type>& operator >> ( simple::file_istream<same_endian_type>& s, std::vector<Column>& v) {
		size_t size;
		s >> size;
		v.resize(size);
		for(int i = 0; i < size; i++)
			s >> v[i];
		return s;
	}


	// Struct representing one piece of data stored in a column
	struct Data {
		// Pointer to the column this piece of data belongs to (must be set for serialization to be possible)
		Column* column = nullptr;
		// // Pointer to the record (row) this piece of data belongs to
		// Record* record;

		// The stored data
		using Variant = std::variant<std::monostate, int64_t, double, std::string>;
		Variant data;

		// Check if the stored data is null
		bool isNull() const { return data.index() == 0; }
		// Construct some null data
		static Data null(Column* column = nullptr) { return {column, {}}; }
	};
	// Data De/serialization
	template<typename same_endian_type> typename simple::file_ostream<same_endian_type>& operator << ( simple::file_ostream<same_endian_type>& s, const Data& d) {
		s << std::byte(d.isNull());
		if(!d.isNull()) {
			// if the data isn't null, we use visit to store the data in the file
			std::visit([&](const auto& data){
				s << data;
			}, d.data);
		}
		return s;
	}
	template<typename same_endian_type> typename simple::file_istream<same_endian_type>& operator >> ( simple::file_istream<same_endian_type>& s, Data& d) {
		std::byte null;
		s >> null;
		if(!bool(null)) {
			// If the data isn't null, we use the column pointer to determine how to deserialize the data
			switch(d.column->type.type){
			break; case DataType::INT: {
				int64_t data;
				s >> data;
				d.data = data;
			}
			break; case DataType::FLOAT: {
				double data;
				s >> data;
				d.data = data;
			}
			break; case DataType::CHAR:
			case DataType::VARCHAR:
			case DataType::TEXT: {
				std::string data;
				s >> data;
				d.data = data;
			}
			}
		} else
			d.data = {};
		return s;
	}


	// Struct representing a row in the table (this class is a thing wrapper around std::vector)
	struct Record: public std::vector<Data> {
		// Pointer to the table this record belongs to
		Table* table = nullptr;

		using std::vector<Data>::vector;
	};
	// Record De/serialization
	template<typename same_endian_type> typename simple::file_ostream<same_endian_type>& operator << ( simple::file_ostream<same_endian_type>& s, const Record& r) {
		s << r.size();
		for(Data& d: r)
			s << d;
		return s;
	}
	template<typename same_endian_type> typename simple::file_istream<same_endian_type>& operator >> ( simple::file_istream<same_endian_type>& s, Record& r) {
		size_t size;
		s >> size;
		r.resize(size);
		for(size_t i = 0; i < size; i++)
			s >> r[i];
		return s;
	}

	// Struct representing a table
	struct Table {
		// Pointer to the database this table belongs to
		Database* database;

		// The name of this table
		std::string name;
		// The path to this table
		std::filesystem::path path;
		// The columns of this table
		std::vector<Column> columns;

		// The records this table is storing
		std::vector<Record> records;
	};
	// Table De/serialization
	template<typename same_endian_type> typename simple::file_ostream<same_endian_type>& operator << ( simple::file_ostream<same_endian_type>& s, const Table& t) {
		return s << "TABLE" << t.name << t.path << t.columns << t.records;
	}
	template<typename same_endian_type> typename simple::file_istream<same_endian_type>& operator >> ( simple::file_istream<same_endian_type>& s, Table& t) {
		std::string table;
		return s >> table >> t.name >> t.path >> t.columns >> t.records;
	}

	// Struct representing a database
	struct Database {
		// The name of this database
		std::string name;

		// The filesystem path to this database
		std::filesystem::path path;
		// Relative filesystem paths to the tables this database manages
		std::vector<std::filesystem::path> tables;

		std::map<std::string, std::shared_ptr<Table>> tableMap; // TODO: should this map, map paths to tables? // TODO: Remove?
	};
	// Database De/serialization
	template<typename same_endian_type> typename simple::file_ostream<same_endian_type>& operator << ( simple::file_ostream<same_endian_type>& s, const Database& d) {
		return s << "DATABASE" << d.name << d.path << d.tables;
	}
	template<typename same_endian_type> typename simple::file_istream<same_endian_type>& operator >> ( simple::file_istream<same_endian_type>& s, Database& d) {
		std::string database;
		return s >> database >> d.name >> d.path >> d.tables;
	}


	// Inline namespaces are optional, sql::ast::Transaction can also be accessed with sql::Transaction (the ast label is used in parser code to avoid ambiguities)
	inline namespace ast {

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
				Query,

				// Actions specific to table alters // TODO: Should we consildate these into add and drop?
				Add,
				Remove,

				MAX
			};
			static const std::array<std::string, Action::MAX> ActionNames; //= {"Invalid", "Use", "Create", "Drop", "Alter", "Query", "Add", "Remove"};

			// Struct which represents the target of this command
			struct Target {
				enum Type {
					Invalid,
					Database,
					Table,
					Column,

					MAX
				};
				static const std::array<std::string, Type::MAX> TypeNames; //= {"Invalid", "Database", "Table", "Column"};

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

		// Memory backing for the enum name arrays
		inline const std::array<std::string, Transaction::Action::MAX> Transaction::ActionNames = {"Invalid", "Use", "Create", "Drop", "Alter", "Query", "Add", "Remove"};
		inline const std::array<std::string, Transaction::Target::MAX> Transaction::Target::TypeNames = {"Invalid", "Database", "Table", "Column"};
	} // ast

} // sql

#endif // SQL_HPP