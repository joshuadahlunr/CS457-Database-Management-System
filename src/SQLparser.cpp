#include "SQLparser.hpp"

#include <lexy/callback.hpp>     // value callbacks
#include <lexy/dsl.hpp>          // lexy::dsl::*
#include <lexy/action/parse.hpp> // lexy::parse
#include <lexy/input/string_input.hpp>
#include <lexy_ext/report_error.hpp> // lexy_ext::report_error

namespace sql::grammar {
	namespace dsl = lexy::dsl;

	// static constexpr auto ws = dsl::unicode::blank; // TODO: Does this bring newlines or do those need to be added manually?
	static constexpr auto ws = dsl::ascii::blank;
	// Rule which captures 0 or more whitespace characters
	static constexpr auto wss = dsl::while_(ws);
	// Rule which captures 1 or more whitespace characters
	static constexpr auto wsp = dsl::while_one(ws);

	// The wildcard token
	static constexpr auto tWildcard = dsl::lit_c<'*'>;
	// Statement termination (stop) token
	static constexpr auto stop = dsl::lit_c<';'>;

	// Upper/Lower case letters
	namespace UL {
		static constexpr auto a = dsl::lit_c<'A'> / dsl::lit_c<'a'>;
		static constexpr auto b = dsl::lit_c<'B'> / dsl::lit_c<'b'>;
		static constexpr auto c = dsl::lit_c<'C'> / dsl::lit_c<'c'>;
		static constexpr auto d = dsl::lit_c<'D'> / dsl::lit_c<'d'>;
		static constexpr auto e = dsl::lit_c<'E'> / dsl::lit_c<'e'>;
		static constexpr auto f = dsl::lit_c<'F'> / dsl::lit_c<'f'>;
		static constexpr auto g = dsl::lit_c<'G'> / dsl::lit_c<'g'>;
		static constexpr auto h = dsl::lit_c<'H'> / dsl::lit_c<'h'>;
		static constexpr auto i = dsl::lit_c<'I'> / dsl::lit_c<'i'>;
		static constexpr auto j = dsl::lit_c<'J'> / dsl::lit_c<'j'>;
		static constexpr auto k = dsl::lit_c<'K'> / dsl::lit_c<'k'>;
		static constexpr auto l = dsl::lit_c<'L'> / dsl::lit_c<'l'>;
		static constexpr auto m = dsl::lit_c<'M'> / dsl::lit_c<'m'>;
		static constexpr auto n = dsl::lit_c<'N'> / dsl::lit_c<'n'>;
		static constexpr auto o = dsl::lit_c<'O'> / dsl::lit_c<'o'>;
		static constexpr auto p = dsl::lit_c<'P'> / dsl::lit_c<'p'>;
		static constexpr auto q = dsl::lit_c<'Q'> / dsl::lit_c<'q'>;
		static constexpr auto r = dsl::lit_c<'R'> / dsl::lit_c<'r'>;
		static constexpr auto s = dsl::lit_c<'S'> / dsl::lit_c<'s'>;
		static constexpr auto t = dsl::lit_c<'T'> / dsl::lit_c<'t'>;
		static constexpr auto u = dsl::lit_c<'U'> / dsl::lit_c<'u'>;
		static constexpr auto v = dsl::lit_c<'V'> / dsl::lit_c<'v'>;
		static constexpr auto w = dsl::lit_c<'W'> / dsl::lit_c<'w'>;
		static constexpr auto x = dsl::lit_c<'X'> / dsl::lit_c<'x'>;
		static constexpr auto y = dsl::lit_c<'Y'> / dsl::lit_c<'y'>;
		static constexpr auto z = dsl::lit_c<'Z'> / dsl::lit_c<'z'>;
	}


	// Rule that matches an identifier
	struct Identifier: lexy::token_production {
		constexpr static auto head = lexy::dsl::unicode::alpha / dsl::lit_c<'_'> / dsl::lit_c<'#'> / dsl::lit_c<'@'>;
		constexpr static auto tail = lexy::dsl::unicode::alnum / dsl::lit_c<'_'> / dsl::lit_c<'#'> / dsl::lit_c<'@'> / dsl::lit_c<'$'>;

		static constexpr auto rule = dsl::peek(head) >> dsl::identifier(head, tail);
		static constexpr auto value = lexy::as_string<std::string> | lexy::callback<std::string>([](std::string&& ident){
			// Ensure that every character in the identifier is lowercase
			for(char& c: ident)
				c = std::tolower(c);

			return ident;
		});

		// A comma seperated list of identifiers
		struct List {
			static constexpr auto rule = dsl::list(dsl::p<Identifier>, dsl::sep(dsl::comma));
			static constexpr auto value = lexy::as_list<std::vector<std::string>>;
		};
	};

	// Rule that matches a string literal
	struct StringLiteral: lexy::token_production {
		struct invalid_char {
			static LEXY_CONSTEVAL auto name() { return "invalid character in string literal"; }
		};

		// A mapping of the simple escape sequences to their replacement values.
		static constexpr auto escaped_symbols = lexy::symbol_table<char> //
			.map<'"'>('"')
			.map<'\\'>('\\')
			.map<'/'>('/')
			.map<'b'>('\b')
			.map<'f'>('\f')
			.map<'n'>('\n')
			.map<'r'>('\r')
			.map<'t'>('\t');

		static constexpr auto rule = [] {
			// Everything is allowed inside a string except for control characters.
			auto code_point = (-dsl::unicode::control).error<invalid_char>;

			// Escape sequences start with a backlash and either map one of the symbols,
			// or a Unicode code point of the form uXXXX.
			auto escape = dsl::backslash_escape //
				.symbol<escaped_symbols>()
				.rule(dsl::lit_c<'u'> >> dsl::code_point_id<4>);

			// String of code_point with specified escape sequences, surrounded by ".
			// We abort string parsing if we see a newline to handle missing closing ".
			return dsl::quoted.limit(dsl::ascii::newline)(code_point, escape) | dsl::single_quoted.limit(dsl::ascii::newline)(code_point, escape);
		}();

		static constexpr auto value = lexy::as_string<std::string, lexy::utf8_encoding>;
	};

	// Rule that matches a complex number (int or float)
	struct NumberLiteral: lexy::token_production {
		// A signed integer parsed as int64_t.
		struct integer: lexy::transparent_production {
			static constexpr auto rule = dsl::minus_sign + dsl::integer<std::int64_t>(dsl::digits<>.no_leading_zero());
			static constexpr auto value = lexy::as_integer<std::int64_t>;
		};

		// The fractional part of a number parsed as the string.
		struct fraction: lexy::transparent_production {
			static constexpr auto rule  = dsl::lit_c<'.'> >> dsl::capture(dsl::digits<>);
			static constexpr auto value = lexy::as_string<std::string>;
		};

		// The exponent of a number parsed as int64_t.
		struct exponent: lexy::transparent_production {
			static constexpr auto rule = UL::e >> dsl::sign + dsl::integer<std::int16_t>(dsl::digits<>);
			static constexpr auto value = lexy::as_integer<std::int16_t>;
		};

		static constexpr auto rule = dsl::peek(dsl::lit_c<'-'> / dsl::digit<>)
			>> dsl::p<integer> + dsl::opt(dsl::p<fraction>) + dsl::opt(dsl::p<exponent>);
		static constexpr auto value = lexy::construct<Number>;
	};

	// Rule that matches a basic integer
	struct IntegerLiteral: lexy::token_production {
		static constexpr auto rule = [] {
			auto digits = dsl::digits<>.sep(dsl::digit_sep_tick).no_leading_zero();
			return dsl::integer<int64_t>(digits);
		}();
		static constexpr auto value = lexy::as_integer<int64_t>;
	};

	// Rule that matches the wildcard token and brings a nullopt value with it
	struct Wildcard {
		static constexpr auto rule = tWildcard;
		static constexpr auto value = lexy::constant(std::nullopt);
	};

	// An identifier
	static constexpr auto identifier = dsl::peek(Identifier::head) >> dsl::p<Identifier>;
	static constexpr auto identifierList = dsl::p<Identifier::List>;
	// A string literal
	static constexpr auto stringLiteral = dsl::peek(LEXY_LIT("\"") / LEXY_LIT("'")) >> dsl::p<StringLiteral>;
	// A numeric literal
	static constexpr auto numberLiteral = dsl::p<NumberLiteral>; // TODO: Needs branch condition?
	// A (explicitly) integer literal
	static constexpr auto integerLiteral = dsl::p<IntegerLiteral>; // TODO: Needs branch condition?
	// Wildcard token (with an attached nullopt value)
	static constexpr auto wildcard = dsl::peek(tWildcard) >> dsl::p<Wildcard>;

	// Keywords
	// select, drop, create, use, alter, add, remove
	// database, table, column
	// from

	namespace KW {
		// --- Action Keywords ---

		// Rule that matches the SELECT keyword
		struct Select: lexy::token_production {
			static constexpr auto rule = UL::s + UL::e + UL::l + UL::e + UL::c + UL::t + ws;
			static constexpr auto value = lexy::constant(ast::Transaction::Action::Query);
		};
		// The SELECT keyword
		static constexpr auto select = dsl::peek(UL::s) >> dsl::p<Select>;

		// Rule that matches the DROP keyword
		struct Drop: lexy::token_production {
			static constexpr auto rule = UL::d + UL::r + UL::o + UL::p + ws;
			static constexpr auto value = lexy::constant(ast::Transaction::Action::Drop);
		};
		// The DROP keyword
		static constexpr auto drop = dsl::peek(UL::d) >> dsl::p<Drop>;

		// Rule that matches the CREATE keyword
		struct Create: lexy::token_production {
			static constexpr auto rule = UL::c + UL::r + UL::e + UL::a + UL::t + UL::e + ws;
			static constexpr auto value = lexy::constant(ast::Transaction::Action::Create);
		};
		// The CREATE keyword
		static constexpr auto create = dsl::peek(UL::c) >> dsl::p<Create>;

		// Rule that matches the USE keyword
		struct Use: lexy::token_production {
			static constexpr auto rule = UL::u + UL::s + UL::e + ws;
			static constexpr auto value = lexy::constant(ast::Transaction::Action::Use);
		};
		// The USE keyword
		static constexpr auto use = dsl::peek(UL::u) >> dsl::p<Use>;

		// Rule that matches the ALTER keyword
		struct Alter: lexy::token_production {
			static constexpr auto rule = UL::a + UL::l + UL::t + UL::e + UL::r + ws;
			static constexpr auto value = lexy::constant(ast::Transaction::Action::Alter);
		};
		// The ALTER keyword
		static constexpr auto alter = dsl::peek(UL::a + UL::l) >> dsl::p<Alter>;

		// Rule that matches the ADD keyword
		struct Add: lexy::token_production {
			static constexpr auto rule = UL::a + UL::d + UL::d + ws;
			static constexpr auto value = lexy::constant(ast::Transaction::Action::Add);
		};
		// The ADD keyword
		static constexpr auto add = dsl::peek(UL::a + UL::d) >> dsl::p<Add>;

		// Rule that matches the REMOVE keyword
		struct Remove: lexy::token_production {
			static constexpr auto rule = UL::r + UL::e + UL::m + UL::o + UL::v + UL::e + ws;
			static constexpr auto value = lexy::constant(ast::Transaction::Action::Remove);
		};
		// The REMOVE keyword
		static constexpr auto remove = dsl::peek(UL::r) >> dsl::p<Remove>;


		// --- Target Keywords ---


		// Rule that matches the DATABASE keyword
		struct Database: lexy::token_production {
			static constexpr auto rule = UL::d + UL::a + UL::t + UL::a + UL::b + UL::a + UL::s + UL::e + ws;
			static constexpr auto value = lexy::constant(ast::Transaction::Target::Database);
		};
		// The DATABASE keyword
		static constexpr auto database = dsl::peek(UL::d) >> dsl::p<Database>;

		// Rule that matches the TABLE keyword
		struct Table: lexy::token_production {
			static constexpr auto rule = UL::t + UL::a + UL::b + UL::l + UL::e + ws;
			static constexpr auto value = lexy::constant(ast::Transaction::Target::Table);
		};
		// The TABLE keyword
		static constexpr auto table = dsl::peek(UL::t) >> dsl::p<Table>;

		// Rule that matches the COLUMN keyword
		struct Column: lexy::token_production {
			static constexpr auto rule = UL::c + UL::o + UL::l + UL::u + UL::m + UL::n + ws;
			static constexpr auto value = lexy::constant(ast::Transaction::Target::Column);
		};
		// The COLUMN keyword
		static constexpr auto column = dsl::peek(UL::c) >>dsl::p<Column>;


		// --- Miscelanious Keywords ---


		// Rule that matches the FROM keyword
		struct From: lexy::token_production {
			static constexpr auto rule = UL::f + UL::r + UL::o + UL::m + ws;
			static constexpr auto value = lexy::noop;
		};
		// The FROM keyword
		static constexpr auto from = dsl::peek(UL::f) >> dsl::p<From>;
	} // KW

	// Types
	// int, float, char, varchar, text

	namespace Type {
		// Rule that matches the INT type
		struct INT: lexy::token_production {
			static constexpr auto rule = UL::i + UL::n + UL::t;
			static constexpr auto value = lexy::constant(DataType{DataType::INT});
		};
		// INT type token
		static constexpr auto tINT = dsl::peek(UL::i) >> dsl::p<INT>;

		// Rule that matches the FLOAT type
		struct FLOAT: lexy::token_production {
			static constexpr auto rule = UL::f + UL::l + UL::o + UL::a + UL::t;
			static constexpr auto value = lexy::constant(DataType{DataType::FLOAT});
		};
		// FLOAT type token
		static constexpr auto tFLOAT = dsl::peek(UL::f) >> dsl::p<FLOAT>;

		// Rule that matches the CHAR type
		struct CHAR: lexy::token_production {
			static constexpr auto rule = UL::c + UL::h + UL::a + UL::r + wss + dsl::lit_c<'('> + integerLiteral + dsl::lit_c<')'>;
			static constexpr auto value = lexy::callback<DataType>([](uint16_t size) {
				return DataType{DataType::CHAR, size};
			});
		};
		// CHAR type token
		static constexpr auto tCHAR = dsl::peek(UL::c) >> dsl::p<CHAR>;

		// Rule that matches the VARCHAR type
		struct VARCHAR: lexy::token_production {
			static constexpr auto rule = UL::v + UL::a + UL::r + UL::c + UL::h + UL::a + UL::r + wss + dsl::lit_c<'('> + integerLiteral + dsl::lit_c<')'>;
			static constexpr auto value = lexy::callback<DataType>([](uint16_t size) {
				return DataType{DataType::VARCHAR, size};
			});
		};
		// VARCHAR type token
		static constexpr auto tVARCHAR = dsl::peek(UL::v) >> dsl::p<VARCHAR>;

		// Rule that matches the TEXT type
		struct TEXT: lexy::token_production {
			static constexpr auto rule = UL::t + UL::e + UL::x + UL::t;
			static constexpr auto value = lexy::constant(DataType{DataType::TEXT});
		};
		// TEXT type token
		static constexpr auto tTEXT = dsl::peek(UL::t) >> dsl::p<TEXT>;


		// Rule with all of the valid types merged together
		static constexpr auto anyType = tINT | tFLOAT | tCHAR | tVARCHAR | tTEXT;
	} // Type


	// A rule that matches a column declaration (an identifier followed by a type)
	struct ColumnDeclaration {
		struct Intermidiate {
			std::string ident;
			DataType type;
		};

		// <id> <type>
		static constexpr auto rule = identifier + Type::anyType;
		static constexpr auto value = lexy::construct<Column>;

		// A comma seperated list of column declarations
		struct List {
			static constexpr auto rule = dsl::list(dsl::p<ColumnDeclaration>, dsl::sep(dsl::comma));
			static constexpr auto value = lexy::as_list<std::vector<Column>>;
		};
	};
	static constexpr auto columnDeclaration = dsl::p<ColumnDeclaration>;
	static constexpr auto columnDeclarationList = dsl::p<ColumnDeclaration::List>;


	// --- Transactions ---


	// Rule that matches a database creation/deletion
	struct DatabaseTransaction {
		// Data aquired from the parse which needs to be rearranged to fit our data structures
		struct Intermidiate {
			ast::Transaction::Action action;
			ast::Transaction::Target::Type type;
			std::string ident;
		};

		// create/drop database <id>;
		static constexpr auto rule = (KW::create | KW::drop) + KW::database + identifier + stop;
		// Convert the parsed result into a Transcation smart pointer (unified type for all transactions)
		static constexpr auto value = lexy::construct<Intermidiate> | lexy::callback<ast::Transaction::ptr>([](Intermidiate&& i) {
			return std::make_unique<ast::Transaction>(ast::Transaction::Base, i.action, ast::Transaction::Target{i.type, i.ident});
		});
	};

	// Rule that matches a database use
	struct UseDatabaseTransaction {
		// Data aquired from the parse which needs to be rearranged to fit our data structures
		struct Intermidiate {
			ast::Transaction::Action action;
			std::string ident;
		};

		// use <id>;
		static constexpr auto rule = KW::use + identifier + stop;
		// Convert the parsed result into a Transcation smart pointer (unified type for all transactions)
		static constexpr auto value = lexy::construct<UseDatabaseTransaction::Intermidiate> | lexy::callback<ast::Transaction::ptr>([](UseDatabaseTransaction::Intermidiate&& i) {
			return std::make_unique<ast::Transaction>(ast::Transaction::Base, i.action, ast::Transaction::Target{ast::Transaction::Target::Database, i.ident});
		});
	};

	// Rule that matches a table drop
	struct DropTableTransaction {
		// Data aquired from the parse which needs to be rearranged to fit our data structures
		struct Intermidiate {
			ast::Transaction::Action action;
			ast::Transaction::Target::Type type;
			std::string ident;
		};

		// drop table <id>;
		static constexpr auto rule = KW::drop + KW::table + identifier + stop;
		// Convert the parsed result into a Transcation smart pointer (unified type for all transactions)
		static constexpr auto value = lexy::construct<Intermidiate> | lexy::callback<ast::Transaction::ptr>([](Intermidiate&& i) {
			return std::make_unique<ast::Transaction>(ast::Transaction::Base, i.action, ast::Transaction::Target{i.type, i.ident});
		});
	};

	// Rule that matches a table create
	struct CreateTableTransaction {
		// Data aquired from the parse which needs to be rearranged to fit our data structures
		struct Intermidiate {
			ast::Transaction::Action action;
			ast::Transaction::Target::Type type;
			std::string ident;
			std::optional<std::vector<Column>> columns;
		};

		// create table <id> [opt](<id> <type>, ...);
		static constexpr auto rule = KW::create + KW::table + identifier + dsl::opt(dsl::lit_c<'('> >> columnDeclarationList + dsl::lit_c<')'>) + stop;
		// Convert the parsed result into a Transcation smart pointer (unified type for all transactions)
		static constexpr auto value = lexy::construct<Intermidiate> | lexy::callback<ast::Transaction::ptr>([](Intermidiate&& i) {
			return std::make_unique<ast::CreateTableTransaction>(ast::CreateTableTransaction{ast::Transaction::CreateTable, i.action, ast::Transaction::Target{i.type, i.ident}, i.columns.value_or(std::vector<Column>{})});
		});
	};

	// Rule that matches a table insert
	struct QueryTableTransaction {
		// Data aquired from the parse which needs to be rearranged to fit our data structures
		struct Intermidiate {
			ast::Transaction::Action action;
			std::optional<std::vector<std::string>> columns;
			std::string ident;
		};

		// select */<id>, ... from <id>;
		static constexpr auto rule = KW::select + (wildcard | identifierList) + KW::from + identifier + stop;
		// Convert the parsed result into a Transcation smart pointer (unified type for all transactions)
		static constexpr auto value = lexy::construct<Intermidiate> | lexy::callback<ast::Transaction::ptr>([](Intermidiate&& i) {
			using wc = sql::Wildcard<std::vector<std::string>>;
			wc columns = i.columns.has_value() ? (wc)i.columns.value() : (wc)std::nullopt;
			return std::make_unique<ast::QueryTableTransaction>(ast::QueryTableTransaction{ast::Transaction::QueryTable, i.action, ast::Transaction::Target{ast::Transaction::Target::Table, i.ident}, columns});
		});
	};

	// Rule that matches a table alter
	struct AlterTableTransaction {
		// Data aquired from the parse which needs to be rearranged to fit our data structures
		struct Intermidiate {
			ast::Transaction::Action action;
			ast::Transaction::Target::Type type;
			std::string ident;
			ast::Transaction::Action alterAction;
			Column alterTarget;
		};

		// alter table <id> add/remove/alter <id> (opt)<type>;
		static constexpr auto rule = KW::alter + KW::table + identifier + (((KW::add | KW::alter) >> columnDeclaration) | (KW::remove >> identifier)) + stop;
		// Convert the parsed result into a Transcation smart pointer (unified type for all transactions)
		static constexpr auto value = lexy::construct<Intermidiate> | lexy::callback<ast::Transaction::ptr>([](Intermidiate&& i) {
			return std::make_unique<ast::AlterTableTransaction>(ast::AlterTableTransaction{ast::Transaction::AlterTable, i.action, ast::Transaction::Target{i.type, i.ident}, i.alterAction, i.alterTarget});
		});
	};

	// Rule that matches any type of transaction and forwards the resulting smart pointer
	struct Transaction {
		static constexpr auto whitespace = ws;
		static constexpr auto rule = dsl::peek(KW::create + KW::database) >> dsl::p<DatabaseTransaction>
			| dsl::peek(KW::create + KW::table) >> dsl::p<CreateTableTransaction>
			| dsl::peek(KW::drop + KW::database) >> dsl::p<DatabaseTransaction>
			| dsl::peek(KW::drop + KW::table) >> dsl::p<DropTableTransaction>
			| dsl::peek(KW::use) >> dsl::p<UseDatabaseTransaction>
			| dsl::peek(KW::select) >> dsl::p<QueryTableTransaction>
			| dsl::peek(KW::alter) >> dsl::p<AlterTableTransaction>;
		static constexpr auto value = lexy::forward<ast::Transaction::ptr>;
	};

} // sql::grammar


sql::ast::Transaction::ptr parseSQL(std::string in) {
	auto result = lexy::parse<sql::grammar::Transaction>(lexy::string_input<lexy::utf8_encoding>(in), lexy_ext::report_error);
	if(result.has_value())
		return std::move(const_cast<sql::ast::Transaction::ptr&>(result.value()));

	return {};
}