/*------------------------------------------------------------
 * Filename: SQL.hpp
 * Author: Joshua Dahl
 * Email: joshuadahl@nevada.unr.edu
 * Created: 2/7/22
 * Modified: 4/13/22
 * Description: Provides several data structs which hold database, tables, columns, tuples, etc...,
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
	struct Tuple;

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
			BOOL,
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
			break; case BOOL:
				return "bool";
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
			break; default:
				throw std::runtime_error("Unknown type");
			}
		}

		// Function that checks if one data type is compatible with another data type
		bool compatibleType(const DataType& other) const {
			switch(type) {
			break; case BOOL: return other.type == BOOL;
			break; case INT: return other.type == INT;
			break; case FLOAT: return other.type == FLOAT;
			break; case CHAR: return other.type == CHAR || other.type == VARCHAR || other.type == TEXT;
			break; case VARCHAR: return other.type == CHAR || other.type == VARCHAR || other.type == TEXT;
			break; case TEXT: return other.type == CHAR || other.type == VARCHAR || other.type == TEXT;
			break; default: return false;
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
		for(size_t i = 0; i < size; i++)
			s >> v[i];
		return s;
	}


	// Struct representing one piece of data stored in a column
	struct Data {
		// The stored data
		using Variant = std::variant<std::monostate, bool, int64_t, double, std::string>;
		Variant data;

		// Pointer to the column this piece of data belongs to (must be set for serialization to be possible)
		Column* column = nullptr;

		// Check if the stored data is null
		bool isNull() const { return data.index() == 0; }
		// Construct some null data
		static Data null(Column* column = nullptr) { return {{}, column}; }

		// Apply adjustments so that the data is valid for the column's data type
		void applyColumnAdjustments() { applyColumnAdjustments(*column, data); }
		static void applyColumnAdjustments(const Column& column, Variant& data) {
			// No adjustments needed if the data is null
			if(data.index() == 0) return;

			switch(column.type.type){
			// No adjustments needed for bool
			break; case DataType::BOOL:
				break;
			// If the column is of type int, convert float data to int
			break; case DataType::INT:
				if(data.index() == 3)
					data = (int64_t) std::get<double>(data);
			// If the column is of type float, convert int data to float
			break; case DataType::FLOAT:
				if(data.index() == 2)
					data = (double) std::get<int64_t>(data);
			break; case DataType::CHAR:{
				std::string str = std::get<std::string>(data);

				// If the string is shorter than the data type, pad it with spaces
				if(str.size() < column.type.size)
					for(size_t i = 0, size = column.type.size - str.size(); i < size; i++)
						str += " ";
				// If the string is longer than the data type, truncate it
				else if(str.size() > column.type.size)
					str = str.substr(0, column.type.size);

				data = str;
			}
			break; case DataType::VARCHAR:{
				std::string str = std::get<std::string>(data);

				// If the string is longer than the data type, truncate it
				if(str.size() > column.type.size)
					str = str.substr(0, column.type.size);

				data = str;

			}
			// No adjustments needed for text
			break; case DataType::TEXT:
				break;
			break; default:
				throw std::runtime_error("Unknown type");
			}
		}

		// Validates that the variant type correctly matches with the column type
		// NOTE: our parser treats floats and ints the same <parserValidation> ensures that data straight from the parser is properly validated
		bool validateVariant(bool parserValidation = false) { return validateVariant(*column, data, parserValidation); }
		static bool validateVariant(const Column& column, const Variant& v, bool parserValidation = false) {
			// Null data is always allowed
			if(v.index() == 0) return true;

			switch(column.type.type){
			break; case DataType::BOOL:
				return v.index() == 1;
			break; case DataType::INT:
				return v.index() == 2 || (v.index() == 3 && parserValidation);
			break; case DataType::FLOAT:
				return (v.index() == 2 && parserValidation) || v.index() == 3;
			break; case DataType::CHAR:
			case DataType::VARCHAR:
			case DataType::TEXT:
				return v.index() == 4;
			break; default:
				throw std::runtime_error("Unknown type");
			}
		}

		static std::string variantTypeString(const Variant& v) {
			switch(v.index()){
			break; case 0: return "Null Literal";
			break; case 1: return "Boolean Literal";
			break; case 2: return "Integer Literal";
			break; case 3: return "Number Literal";
			break; case 4: return "String Literal";
			break; default:
				throw std::runtime_error("Variant in invalid state");
			}
		}
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
			break; case DataType::BOOL: {
				bool data;
				s >> data;
				d.data = data;
			}
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
			break; default:
				throw std::runtime_error("Unexpected data type");
			}
		} else
			d.data = {};
		return s;
	}


	// Struct representing a row in the table (this class is a thin wrapper around std::vector)
	struct Tuple: public std::vector<Data> {
		// Pointer to the table this tuple belongs to
		Table* table = nullptr;

		using std::vector<Data>::vector;
	};
	// Tuple De/serialization
	template<typename same_endian_type> typename simple::file_ostream<same_endian_type>& operator << (simple::file_ostream<same_endian_type>& s, const Tuple& t) {
		s << t.size();
		for(const Data& d: t)
			s << d;
		return s;
	}
	template<typename same_endian_type> typename simple::file_istream<same_endian_type>& operator >> (simple::file_istream<same_endian_type>& s, Tuple& t) {
		size_t size;
		s >> size;
		t.resize(size);
		for(size_t i = 0; i < size; i++)
			s >> t[i];
		return s;
	}
	// Vector of tuple De/serialization
	template<typename same_endian_type> typename simple::file_ostream<same_endian_type>& operator << ( simple::file_ostream<same_endian_type>& s, const std::vector<Tuple>& v) {
		s << v.size();
		for(const Tuple& c: v)
			s << c;
		return s;
	}
	template<typename same_endian_type> typename simple::file_istream<same_endian_type>& operator >> ( simple::file_istream<same_endian_type>& s, std::vector<Tuple>& v) {
		size_t size;
		s >> size;
		v.resize(size);
		for(size_t i = 0; i < size; i++)
			s >> v[i];
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

		// The tuples this table is storing
		std::vector<Tuple> tuples;

		// Function which creates a new tuple
		Tuple& createEmptyTuple(){
			tuples.emplace_back();
			Tuple& out = tuples.back();
			out.table = this;

			for(Column& column: columns)
				out.emplace_back(std::move(Data::null(&column)));
			return out;
		}
	};
	// Table De/serialization
	template<typename same_endian_type> typename simple::file_ostream<same_endian_type>& operator << ( simple::file_ostream<same_endian_type>& s, const Table& t) {
		return s << "TABLE" << t.name << t.path << t.columns << t.tuples;
	}
	template<typename same_endian_type> typename simple::file_istream<same_endian_type>& operator >> ( simple::file_istream<same_endian_type>& s, Table& t) {
		std::string table;
		size_t numTuples;
		s >> table >> t.name >> t.path >> t.columns >> numTuples;
		for(int i = 0; i < numTuples; i++) {
			Tuple& tuple = t.createEmptyTuple();
			s >> tuple;
		}

		return s;
	}

	// Struct representing a database
	struct Database {
		// The name of this database
		std::string name;

		// The filesystem path to this database
		std::filesystem::path path;
		// Relative filesystem paths to the tables this database manages
		std::vector<std::filesystem::path> tables;
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

			// Actions a transaction can be performing
			enum Action {
				Invalid,
				Use,
				Create,
				Drop,
				Alter,
				Insert,
				Update,
				Delete,
				Query,

				// Actions specific to table alters // TODO: Should we consildate these into create and drop?
				Add,
				Remove,

				MAX
			};
			static const std::array<std::string, Action::MAX> ActionNames; //= {"Invalid", "Use", "Create", "Drop", "Alter", "Insert", "Update", "Delete", "Query", "Add", "Remove"};

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

			// The action to be taken by this command
			Action action;
			// The target of this command
			Target target;
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

		// Struct representing a transaction that inserts a new tuple into the table
		struct InsertIntoTableTransaction: public Transaction {
			// The values to be inserted
			std::vector<Data::Variant> values;
		};


		// Struct representing a transaction with a set of where clauses
		struct WhereTransaction: public Transaction {
			enum Comparison {
				equal,
				notEqual,
				less,
				greater,
				lessEqual,
				greaterEqual,
			};

			struct Condition {
				using Variant = std::variant<std::monostate, bool, int64_t, double, std::string, Column>;
				std::string column;
				Comparison comp;
				Variant value;
			};

			std::vector<Condition> conditions;
		};
		// Function that flattens a variant of Data::Variant or a Column into a where condition variant
		static WhereTransaction::Condition::Variant flatten(std::variant<Column, Data::Variant> v) {
			if(v.index() == 0)
				return std::get<Column>(v);
			else {
				auto& dv = std::get<Data::Variant>(v);
				switch(dv.index()){
				break; case 0: return {};
				break; case 1: return std::get<bool>(dv);
				break; case 2: return std::get<int64_t>(dv);
				break; case 3: return std::get<double>(dv);
				break; case 4: return std::get<std::string>(dv);
				break; default: throw std::runtime_error("Unexpected data type");
				}
			}
		}
		// Function that extracts a Data::Variant from a where condition variant
		static Data::Variant extractData(WhereTransaction::Condition::Variant v) {
			switch(v.index()){
			break; case 0: return {};
			break; case 1: return std::get<bool>(v);
			break; case 2: return std::get<int64_t>(v);
			break; case 3: return std::get<double>(v);
			break; case 4: return std::get<std::string>(v);
			break; case 5: return {}; // Columns are treated as being null data for validation purposes
			break; default: throw std::runtime_error("Unexpected data type");
			}
		}

		// Struct representing a table query transaction
		struct QueryTableTransaction: public WhereTransaction {
			struct TableAlias {
				// The name of the table
				std::string table;
				// The alias the table is known by in the query
				std::string alias;

				// The type of join
				enum JoinType {
					Inner,
					Left
				};
				JoinType joinType = Inner;

				// Function which returns true if this is an outer join
				bool isOuterJoin() { return joinType != Inner; }
			};
			// A list of tables that should be joined to construct this query
			std::vector<TableAlias> tableAliases;

			// The columns (or wildcard) to query
			Wildcard<std::vector<std::string>> columns;
		};

		// Struct representing a transaction that updates some values in the table
		struct UpdateTableTransaction: public WhereTransaction {
			// Name of the column to be updated
			std::string column;
			// The value to update in that column
			Data::Variant value;
		};

		// Struct representing a transaction that deletes some values from the table
		struct DeleteFromTableTransaction: public WhereTransaction {};

		// Memory backing for the enum name arrays
		inline const std::array<std::string, Transaction::Action::MAX> Transaction::ActionNames = {"Invalid", "Use", "Create", "Drop", "Alter", "Insert", "Update", "Delete", "Query", "Add", "Remove"};
		inline const std::array<std::string, Transaction::Target::MAX> Transaction::Target::TypeNames = {"Invalid", "Database", "Table", "Column"};
	} // ast

} // sql

#endif // SQL_HPP