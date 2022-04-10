/*------------------------------------------------------------
 * Filename: SQLParser.cpp
 * Author: Joshua Dahl
 * Email: joshuadahl@nevada.unr.edu
 * Created: 2/7/22
 * Modified: 3/14/22
 * Description: File which implements the grammar for parsing SQL (implemented as a Lexy DSL)
 *------------------------------------------------------------*/

#include "SQLparser.hpp"

#include <lexy/action/parse.hpp> // lexy::parse
#include <lexy/callback.hpp>     // value callbacks
#include <lexy/dsl.hpp>          // lexy::dsl::*
#include <lexy/input/string_input.hpp>
#include <lexy_ext/report_error.hpp> // lexy_ext::report_error

namespace sql::grammar {
	namespace dsl = lexy::dsl;

	// Whitespace is blank characters and newlines
	static constexpr auto ws = dsl::ascii::blank / dsl::newline;
	static constexpr auto comment = LEXY_LIT("--") >> dsl::until(dsl::newline);
	static constexpr auto wsc = ws | comment;
	// Rule which captures 0 or more whitespace characters
	static constexpr auto wss = dsl::while_(wsc);
	// Rule which captures 1 or more whitespace characters
	static constexpr auto wsp = dsl::while_one(wsc);

	// The wildcard token
	static constexpr auto tWildcard = dsl::lit_c<'*'>;
	// Statement termination (stop) token
	static constexpr auto stop = dsl::lit_c<';'>;

	// Upper/Lower case letters
	namespace UpperLower {
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
	namespace UL = UpperLower;


	// Rule that matches an identifier (as defined in Microsoft's SQL definition)
	struct Identifier: lexy::token_production {
		constexpr static auto head = dsl::unicode::alpha / dsl::lit_c<'_'> / dsl::lit_c<'#'> / dsl::lit_c<'@'>;
		constexpr static auto tail = dsl::unicode::alnum / dsl::lit_c<'_'> / dsl::lit_c<'#'> / dsl::lit_c<'@'> / dsl::lit_c<'$'> / dsl::lit_c<'.'>;

		static constexpr auto rule = dsl::peek(head) >> dsl::identifier(head, tail);
		static constexpr auto value = lexy::as_string<std::string>;

		// A comma separated list of identifiers
		struct List {
			static constexpr auto rule = dsl::list(dsl::p<Identifier>, dsl::sep(dsl::comma));
			static constexpr auto value = lexy::as_list<std::vector<std::string>>;
		};
	};

	namespace literal {
		// Parses a numeric literal
		struct number : lexy::token_production {
			// Prefix indicating the base of the number
			struct base_prefix : lexy::transparent_production {
				static constexpr auto rule = dsl::opt(dsl::capture(LEXY_LIT("0x") / LEXY_LIT("0X") / LEXY_LIT("0b") / LEXY_LIT("0B") / LEXY_LIT("0")));
				static constexpr auto value = lexy::as_string<std::string> | lexy::callback<uint8_t>([](auto prefix) -> uint8_t {
					if(prefix.empty())
						return 10; // Decimal
					else if(prefix == "0x")
						return 16; // Hex
					else if(prefix == "0X")
						return 16; // Hex
					else if(prefix == "0b")
						return 2; // Binary
					else if(prefix == "0B")
						return 2; // Binary
					else if(prefix == "0")
						return 8; // Octal
					return 10;
				});
			};

			template<typename Type = dsl::hex>
			static constexpr auto digit = dsl::digit<Type> / dsl::lit_c<'_'> / dsl::lit_c<'\''>;

			// A signed integer parsed as int64_t.
			struct hex_integer : lexy::transparent_production {
				static constexpr auto rule = dsl::capture(dsl::while_one(digit<dsl::hex>));
				static constexpr auto value = lexy::as_string<std::string>;
			};
			struct decimal_integer : lexy::transparent_production {
				static constexpr auto rule = dsl::capture(dsl::while_one(digit<dsl::decimal>));
				static constexpr auto value = lexy::as_string<std::string>;
			};

			// + or - sign
			struct sign : lexy::transparent_production {
				enum type {
					plus,
					minus,
				};

				static constexpr auto rule = dsl::capture(dsl::lit_c<'+'> / dsl::lit_c<'-'>);
				static constexpr auto value = lexy::as_string<std::string> | lexy::callback<type>([](auto sign) -> type {
					return sign[0] == '+' ? type::plus : type::minus;
				});
			};

			// Intermediate structure storing parsed results which are then assembled into a numeric value
			struct intermediate {
				std::optional<sign::type> integerSign;
				uint8_t base;
				std::optional<std::string_view> integer;
				std::optional<std::string_view> fraction;
				std::optional<sign::type> exponentSign;
				std::optional<std::string_view> exponent;
			};


			static constexpr auto rule = [](){
				auto exp_char = dsl::lit_c<'e'> | dsl::lit_c<'E'>;

				auto hexInt = dsl::peek(LEXY_LIT("0x") / LEXY_LIT("0X")) >> (dsl::p<base_prefix> + dsl::p<hex_integer>);
				auto nonHexReal = dsl::peek(LEXY_LIT("0b") / LEXY_LIT("0B") / digit<dsl::decimal>) >> (dsl::p<base_prefix> + dsl::opt(dsl::p<decimal_integer>)
					+ dsl::opt(dsl::lit_c<'.'> >> dsl::p<decimal_integer>) // Fraction
					+ dsl::opt(exp_char >> dsl::opt(dsl::p<sign>) + dsl::p<hex_integer>)); // Exponent
				return dsl::opt(dsl::p<sign>) + (hexInt | nonHexReal);
			}();
			static constexpr auto value = lexy::construct<intermediate> | lexy::callback<double>([](auto in) -> double {
				// Convert a char into the hexadecimal value it represents
				auto val = [](char c) -> uint8_t {
					if (c >= '0' && c <= '9') return (uint8_t)c - '0';
					else if(c >= 'A' && c <= 'Z') return (uint8_t)c - 'A' + 10;
					else if(c >= 'a' && c <= 'z') return (uint8_t)c - 'a' + 10;
					return 0;
				};
				// Convert the provided string of digits to a number of the provided base
				auto toDecimal = [val](std::string_view str, uint8_t base) {
					double power = 1; // Initialize power of base
					double num = 0;  // Initialize result

					// Decimal equivalent is str[len-1]*1 +
					// str[len-2]*base + str[len-3]*(base^2) + ...
					for (size_t i = str.size() - 1; i >= 0 && i < (size_t)-1; i--) {
						// Skip underscores and single quotes
						if(str[i] == '_' || str[i] == '\'')
							continue;

						// A digit in input number must be
						// less than number's base
						if (val(str[i]) >= base)
							throw std::runtime_error(std::string("Invalid digit `") + str[i] + "` in base " + std::to_string(base) + " number");

						num += val(str[i]) * power;
						power = power * base;
					}

					return num;
				};
				// Convert the provided string of digits to a fraction of 1 / the provided base
				auto toDecimalFraction = [val](std::string_view str, uint8_t base) {
					double power = 1; power /= base; // Initialize power to 1/base
					double num = 0;  // Initialize result

					// Decimal equivalent is str[len-1]*1/base +
					// str[len-2]*1/(base^2) + str[len-3]*1/(base^3) + ...
					for (size_t i = str.size() - 1; i >= 0 && i < (size_t)-1; i--) {
						// Skip underscores and single quotes
						if(str[i] == '_' || str[i] == '\'')
							continue;

						// A digit in input number must be
						// less than number's base
						if (val(str[i]) >= base)
							throw std::runtime_error(std::string("Invalid digit `") + str[i] + "` in base " + std::to_string(base) + " number");

						num += val(str[i]) * power;
						power = power / base;
					}

					return num;
				};


				double fractionValue = 0;
				double exponentValue = 1;

				// Convert the integer portion of the number
				if(in.integer.has_value())
					fractionValue = toDecimal(*in.integer, in.base);
				else if(in.base == 8) { // If we have no integer portion and the base is 8, that means there is a sinle floating 0
					in.base = 10;
					fractionValue = 0;
				} else
					throw std::runtime_error("Number is required after prefix specifier");

				// Convert the optional fractional portion of the number
				if(in.fraction.has_value())
					fractionValue += toDecimalFraction(*in.fraction, in.base);
				// Negate the number if nessicary
				if(in.integerSign.has_value() && *in.integerSign == sign::type::minus)
					fractionValue = -fractionValue;

				// Convert the exponential portion of the number
				if(in.exponent.has_value()) {
					size_t exponentRaw = toDecimal(*in.exponent, in.base);
					for(size_t i = 0; i < exponentRaw; i++)
						exponentValue *= in.base;

					// Support negative exponents
					if(in.exponentSign.has_value() && *in.exponentSign == sign::type::minus)
						exponentValue = 1 / exponentValue;
				}

				return fractionValue * exponentValue;
			});
		};


		// Parses an arbitrary string literal
		struct string : lexy::token_production {
			struct invalid_char {
				static LEXY_CONSTEVAL auto name() { return "invalid character in string literal"; }
			};

			// A mapping of the simple escape sequences to their replacement values.
			static constexpr auto escaped_symbols = lexy::symbol_table<char>
													.map<'"'>('"')
													.map<'\''>('\'')
													.map<'\?'>('?')
													.map<'\\'>('\\')
													.map<'/'>('/')
													.map<'a'>('\a')
													.map<'b'>('\b')
													.map<'f'>('\f')
													.map<'n'>('\n')
													.map<'r'>('\r')
													.map<'t'>('\t')
													.map<'v'>('\v');
													// .map<'0'>('\0');

			static constexpr auto rule = [] {
				// Everything is allowed inside a string except for control characters.
				auto code_point = (-dsl::unicode::control).error<invalid_char>;

				// Escape sequences start with a backlash and either map one of the symbols,
				// or a Unicode code point of the form uXXXX.
				auto escape = dsl::backslash_escape //
								.symbol<escaped_symbols>()
								.rule(dsl::peek(dsl::digit<lexy::dsl::octal>) >> (dsl::code_point_id<3, dsl::octal> | dsl::code_point_id<2, dsl::octal>
									| dsl::integer<lexy::code_point, dsl::octal>(dsl::lit_c<'0'> / dsl::lit_c<'1'> / dsl::lit_c<'2'> / dsl::lit_c<'3'> / dsl::lit_c<'4'> / dsl::lit_c<'5'> / dsl::lit_c<'6'> / dsl::lit_c<'7'>)))
								.rule(dsl::lit_c<'x'> >> dsl::code_point_id<2, dsl::hex>)
								.rule(dsl::lit_c<'u'> >> dsl::code_point_id<4>)
								.rule(dsl::lit_c<'U'> >> dsl::code_point_id<8>);

				// String of code_point with specified escape sequences, surrounded by ".
				// We abort string parsing if we see a newline to handle missing closing ".
				return dsl::quoted.limit(dsl::ascii::newline)(code_point, escape)
					| dsl::single_quoted.limit(dsl::ascii::newline)(code_point, escape);
			}();

			static constexpr auto value = lexy::as_string<std::string, lexy::utf8_encoding>;
		};


		// A boolean value
		struct boolean : lexy::token_production {
			struct true_ : lexy::transparent_production {
				static constexpr auto rule = UL::t + UL::r + UL::u + UL::e;
				static constexpr auto value = lexy::constant(true);
			};
			struct false_ : lexy::transparent_production {
				static constexpr auto rule = UL::f + UL::a + UL::l + UL::s + UL::e;
				static constexpr auto value = lexy::constant(false);
			};

			static constexpr auto rule = (dsl::peek(UL::t) >> dsl::p<true_>) | (dsl::peek(UL::f) >> dsl::p<false_>);
			static constexpr auto value = lexy::forward<bool>;
		};


		// A null value
		struct null : lexy::token_production {
			static constexpr auto rule = UL::n + UL::u + UL::l + UL::l;
			static constexpr auto value = lexy::constant(std::monostate{});
		};


		// A variant comprised of every possible literal value
		struct variant {
			static constexpr auto rule = (dsl::peek(dsl::lit_c<'\"'> / dsl::lit_c<'\''>) >> dsl::p<string>) | (dsl::peek(UL::t / UL::f) >> dsl::p<boolean>) | (dsl::peek(dsl::digit<dsl::hex>) >> dsl::p<number>) | (dsl::peek(UL::n) >> dsl::p<null>);
			static constexpr auto value = lexy::construct<Data::Variant>;
		};
	} // sql::grammar::literal

	// Rule that matches the wildcard token and brings a nullopt value with it
	struct Wildcard {
		static constexpr auto rule = tWildcard;
		static constexpr auto value = lexy::constant(std::nullopt);
	};

	// An identifier
	static constexpr auto identifier = dsl::peek(Identifier::head) >> dsl::p<Identifier>;
	static constexpr auto identifierList = dsl::p<Identifier::List>;
	// A string literal
	static constexpr auto stringLiteral = dsl::peek(dsl::lit_c<'\"'> / dsl::lit_c<'\''>) >> dsl::p<literal::string>;
	// A numeric literal
	static constexpr auto numberLiteral = dsl::peek(dsl::digit<dsl::hex>) >> dsl::p<literal::number>; // TODO: Needs branch condition?
	// A boolean literal
	static constexpr auto booleanLiteral = dsl::peek(UL::t / UL::f) >> dsl::p<literal::boolean>;
	// A null literal
	static constexpr auto nullLiteral = dsl::peek(UL::n) >> dsl::p<literal::null>;
	// A variant of literals
	static constexpr auto literalVariant = dsl::p<literal::variant>;
	// Wildcard token (with an attached nullopt value)
	static constexpr auto wildcard = dsl::peek(tWildcard) >> dsl::p<Wildcard>;

	// Keywords
	// select, drop, create, use, alter, add, remove
	// database, table, column
	// from

	namespace Keyword {
		// --- Action Keywords ---

		// Rule that matches the SELECT keyword
		struct Select: lexy::token_production {
			static constexpr auto rule = UL::s + UL::e + UL::l + UL::e + UL::c + UL::t + wsc;
			static constexpr auto value = lexy::constant(ast::Transaction::Action::Query);
		};
		// The SELECT keyword
		static constexpr auto select = dsl::peek(UL::s) >> dsl::p<Select>;

		// Rule that matches the DROP keyword
		struct Drop: lexy::token_production {
			static constexpr auto rule = UL::d + UL::r + UL::o + UL::p + wsc;
			static constexpr auto value = lexy::constant(ast::Transaction::Action::Drop);
		};
		// The DROP keyword
		static constexpr auto drop = dsl::peek(UL::d + UL::r) >> dsl::p<Drop>;

		// Rule that matches the DELETE keyword
		struct Delete_: lexy::token_production {
			static constexpr auto rule = UL::d + UL::e + UL::l + UL::e + UL::t + UL::e + ws;
			static constexpr auto value = lexy::constant(ast::Transaction::Action::Delete);
		};
		// The DELETE keyword
		static constexpr auto Delete = dsl::peek(UL::d + UL::e) >> dsl::p<Delete_>;

		// Rule that matches the CREATE keyword
		struct Create: lexy::token_production {
			static constexpr auto rule = UL::c + UL::r + UL::e + UL::a + UL::t + UL::e + wsc;
			static constexpr auto value = lexy::constant(ast::Transaction::Action::Create);
		};
		// The CREATE keyword
		static constexpr auto create = dsl::peek(UL::c) >> dsl::p<Create>;

		// Rule that matches the USE keyword
		struct Use: lexy::token_production {
			static constexpr auto rule = UL::u + UL::s + UL::e + wsc;
			static constexpr auto value = lexy::constant(ast::Transaction::Action::Use);
		};
		// The USE keyword
		static constexpr auto use = dsl::peek(UL::u + UL::s) >> dsl::p<Use>;

		// Rule that matches the UPDATE keyword
		struct Update: lexy::token_production {
			static constexpr auto rule = UL::u + UL::p + UL::d + UL::a + UL::t + UL::e + ws;
			static constexpr auto value = lexy::constant(ast::Transaction::Action::Update);
		};
		// The UPDATE keyword
		static constexpr auto update = dsl::peek(UL::u + UL::p) >> dsl::p<Update>;

		// Rule that matches the INSERT keyword
		struct Insert: lexy::token_production {
			static constexpr auto rule = UL::i + UL::n + UL::s + UL::e + UL::r + UL::t + ws;
			static constexpr auto value = lexy::constant(ast::Transaction::Action::Insert);
		};
		// The INSERT keyword
		static constexpr auto insert = dsl::peek(UL::i) >> dsl::p<Insert>;

		// Rule that matches the ALTER keyword
		struct Alter: lexy::token_production {
			static constexpr auto rule = UL::a + UL::l + UL::t + UL::e + UL::r + wsc;
			static constexpr auto value = lexy::constant(ast::Transaction::Action::Alter);
		};
		// The ALTER keyword
		static constexpr auto alter = dsl::peek(UL::a + UL::l) >> dsl::p<Alter>;

		// Rule that matches the ADD keyword
		struct Add: lexy::token_production {
			static constexpr auto rule = UL::a + UL::d + UL::d + wsc;
			static constexpr auto value = lexy::constant(ast::Transaction::Action::Add);
		};
		// The ADD keyword
		static constexpr auto add = dsl::peek(UL::a + UL::d) >> dsl::p<Add>;

		// Rule that matches the REMOVE keyword
		struct Remove: lexy::token_production {
			static constexpr auto rule = UL::r + UL::e + UL::m + UL::o + UL::v + UL::e + wsc;
			static constexpr auto value = lexy::constant(ast::Transaction::Action::Remove);
		};
		// The REMOVE keyword
		static constexpr auto remove = dsl::peek(UL::r) >> dsl::p<Remove>;


		// --- Target Keywords ---


		// Rule that matches the DATABASE keyword
		struct Database: lexy::token_production {
			static constexpr auto rule = UL::d + UL::a + UL::t + UL::a + UL::b + UL::a + UL::s + UL::e + wsc;
			static constexpr auto value = lexy::constant(ast::Transaction::Target::Database);
		};
		// The DATABASE keyword
		static constexpr auto database = dsl::peek(UL::d) >> dsl::p<Database>;

		// Rule that matches the TABLE keyword
		struct Table: lexy::token_production {
			static constexpr auto rule = UL::t + UL::a + UL::b + UL::l + UL::e + wsc;
			static constexpr auto value = lexy::constant(ast::Transaction::Target::Table);
		};
		// The TABLE keyword
		static constexpr auto table = dsl::peek(UL::t) >> dsl::p<Table>;

		// Rule that matches the COLUMN keyword
		struct Column: lexy::token_production {
			static constexpr auto rule = UL::c + UL::o + UL::l + UL::u + UL::m + UL::n + wsc;
			static constexpr auto value = lexy::constant(ast::Transaction::Target::Column);
		};
		// The COLUMN keyword
		static constexpr auto column = dsl::peek(UL::c) >>dsl::p<Column>;


		// --- Miscelanious Keywords ---


		// Rule that matches the FROM keyword
		struct From: lexy::token_production {
			static constexpr auto rule = UL::f + UL::r + UL::o + UL::m + wsc;
			static constexpr auto value = lexy::noop;
		};
		// The FROM keyword
		static constexpr auto from = dsl::peek(UL::f) >> dsl::p<From>;

		// Rule that matches the WHERE keyword
		struct Where: lexy::token_production {
			static constexpr auto rule = UL::w + UL::h + UL::e + UL::r + UL::e + wsc;
			static constexpr auto value = lexy::noop;
		};
		// The WHERE keyword
		static constexpr auto where = dsl::peek(UL::w) >> dsl::p<Where>;

		// Rule that matches the AND keyword
		struct And_: lexy::token_production {
			static constexpr auto rule = ((UL::a >> UL::n + UL::d) | dsl::lit_c<'&'>) + wsc;
			static constexpr auto value = lexy::noop;
		};
		// The AND keyword
		static constexpr auto And = dsl::peek(UL::a) >> dsl::p<And_>;

		// Rule that matches the INTO keyword
		struct Into: lexy::token_production {
			static constexpr auto rule = UL::i + UL::n + UL::t + UL::o + wsc;
			static constexpr auto value = lexy::noop;
		};
		// The INTO keyword
		static constexpr auto into = dsl::peek(UL::i) >> dsl::p<Into>;

		// Rule that matches the VALUES keyword
		struct Values: lexy::token_production {
			static constexpr auto rule = UL::v + UL::a + UL::l + UL::u + UL::e + dsl::opt(UL::s);
			static constexpr auto value = lexy::noop;
		};
		// The INTO keyword
		static constexpr auto values = dsl::peek(UL::v) >> dsl::p<Values>;

		// Rule that matches the SET keyword
		struct Set: lexy::token_production {
			static constexpr auto rule = UL::s + UL::e + UL::t + wsc;
			static constexpr auto value = lexy::noop;
		};
		// The SET keyword
		static constexpr auto set = dsl::peek(UL::s) >> dsl::p<Set>;
	} // Keyword
	namespace KW = Keyword;


	// Types
	// bool, int, float, char, varchar, text

	namespace Type {
		// Rule that matches the BOOL type
		struct BOOL: lexy::token_production {
			static constexpr auto rule = UL::b + UL::o + UL::o + UL::l;
			static constexpr auto value = lexy::constant(DataType{DataType::BOOL});
		};
		// BOOL type token
		static constexpr auto tBOOL = dsl::peek(UL::b) >> dsl::p<BOOL>;

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
			static constexpr auto rule = UL::c + UL::h + UL::a + UL::r + wss + dsl::lit_c<'('> + numberLiteral + dsl::lit_c<')'>;
			static constexpr auto value = lexy::callback<DataType>([](uint16_t size) {
				return DataType{DataType::CHAR, size};
			});
		};
		// CHAR type token
		static constexpr auto tCHAR = dsl::peek(UL::c) >> dsl::p<CHAR>;

		// Rule that matches the VARCHAR type
		struct VARCHAR: lexy::token_production {
			static constexpr auto rule = UL::v + UL::a + UL::r + UL::c + UL::h + UL::a + UL::r + wss + dsl::lit_c<'('> + numberLiteral + dsl::lit_c<')'>;
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
		static constexpr auto anyType = tBOOL | tINT | tFLOAT | tCHAR | tVARCHAR | tTEXT;
	} // Type



	// A rule that matches a column declaration (an identifier followed by a type)
	struct ColumnDeclaration {
		// <id> <type>
		static constexpr auto rule = identifier + Type::anyType;
		static constexpr auto value = lexy::construct<Column>;

		// A comma separated list of column declarations
		struct List {
			static constexpr auto rule = dsl::list(dsl::p<ColumnDeclaration>, dsl::sep(dsl::comma));
			static constexpr auto value = lexy::as_list<std::vector<Column>>;
		};
	};
	static constexpr auto columnDeclaration = dsl::p<ColumnDeclaration>;
	static constexpr auto columnDeclarationList = dsl::p<ColumnDeclaration::List>;


	// A rule that matches a where condition (an identifier followed by a comparison operator followed by a value literal)
	struct WhereCondition {
		// Structs that parse a comparison operator
		struct EqualComparison {
			static constexpr auto rule = dsl::lit_c<'='>;
			static constexpr auto value = lexy::constant(WhereTransaction::equal);
		};
		struct NotEqualComparison {
			static constexpr auto rule = LEXY_LIT("!=");
			static constexpr auto value = lexy::constant(WhereTransaction::notEqual);
		};
		struct LessComparison {
			static constexpr auto rule = dsl::lit_c<'<'>;
			static constexpr auto value = lexy::constant(WhereTransaction::less);
		};
		struct GreaterComparison {
			static constexpr auto rule = dsl::lit_c<'>'>;
			static constexpr auto value = lexy::constant(WhereTransaction::greater);
		};
		struct LessEqualComparison {
			static constexpr auto rule = LEXY_LIT("<=");
			static constexpr auto value = lexy::constant(WhereTransaction::lessEqual);
		};
		struct GreaterEqualComparison {
			static constexpr auto rule = LEXY_LIT(">=");
			static constexpr auto value = lexy::constant(WhereTransaction::greaterEqual);
		};
		// Struct that wraps an identifier into a sql::Column
		struct ColumnIdentifier {
			static constexpr auto rule = identifier;
			static constexpr auto value = lexy::callback<sql::Column>([](auto&& string) {
				sql::Column c;
				c.name = string;
				return c;
			});
		};

		// Intermediate struct holding parsed results before they are transformed into a condition
		struct Intermediate {
			std::string column;
			WhereTransaction::Comparison comparison;
			std::variant<Column, Data::Variant> value;
		};

		// <id> (= | != | < | > | <= | >=) (<string> | <number> | <bool> | <null> | <id>)
		static constexpr auto rule = identifier + (dsl::p<EqualComparison> | dsl::p<NotEqualComparison> | dsl::p<LessComparison> | dsl::p<GreaterComparison> | dsl::p<LessEqualComparison> | dsl::p<GreaterEqualComparison>) + (literalVariant | dsl::p<ColumnIdentifier>);
		static constexpr auto value = lexy::construct<Intermediate> | lexy::callback<WhereTransaction::Condition>([](Intermediate&& in){
			WhereTransaction::Condition out;
			out.column = in.column;
			out.comp = in.comparison;
			out.value = flatten(in.value);
			return out;
		});

		// A AND separated list of conditions
		struct List {
			static constexpr auto rule = dsl::list(dsl::p<WhereCondition>, dsl::sep(KW::And));
			static constexpr auto value = lexy::as_list<std::vector<WhereTransaction::Condition>>;
		};
	};
	static constexpr auto whereCondition = dsl::p<WhereCondition>;
	static constexpr auto whereConditionList = dsl::p<WhereCondition::List>;
	static constexpr auto whereConditions = KW::where >> whereConditionList;


	// --- Transactions ---


	// Rule that matches a database creation/deletion
	struct DatabaseTransaction {
		// Data acquired from the parse which needs to be rearranged to fit our data structures
		struct Intermediate {
			ast::Transaction::Action action;
			ast::Transaction::Target::Type type;
			std::string ident;
		};

		// create/drop database <id>;
		static constexpr auto rule = (KW::create | KW::drop) + KW::database + identifier + stop;
		// Convert the parsed result into a Transcation smart pointer (unified type for all transactions)
		static constexpr auto value = lexy::construct<Intermediate> | lexy::callback<ast::Transaction::ptr>([](Intermediate&& i) {
			return std::make_unique<ast::Transaction>(ast::Transaction{i.action, ast::Transaction::Target{i.type, i.ident}});
		});
	};

	// Rule that matches a database use
	struct UseDatabaseTransaction {
		// Data acquired from the parse which needs to be rearranged to fit our data structures
		struct Intermediate {
			ast::Transaction::Action action;
			std::string ident;
		};

		// use <id>;
		static constexpr auto rule = KW::use + identifier + stop;
		// Convert the parsed result into a Transcation smart pointer (unified type for all transactions)
		static constexpr auto value = lexy::construct<UseDatabaseTransaction::Intermediate> | lexy::callback<ast::Transaction::ptr>([](UseDatabaseTransaction::Intermediate&& i) {
			return std::make_unique<ast::Transaction>(ast::Transaction{i.action, ast::Transaction::Target{ast::Transaction::Target::Database, i.ident}});
		});
	};

	// Rule that matches a table drop
	struct DropTableTransaction {
		// Data acquired from the parse which needs to be rearranged to fit our data structures
		struct Intermediate {
			ast::Transaction::Action action;
			ast::Transaction::Target::Type type;
			std::string ident;
		};

		// drop table <id>;
		static constexpr auto rule = KW::drop + KW::table + identifier + stop;
		// Convert the parsed result into a Transcation smart pointer (unified type for all transactions)
		static constexpr auto value = lexy::construct<Intermediate> | lexy::callback<ast::Transaction::ptr>([](Intermediate&& i) {
			return std::make_unique<ast::Transaction>(ast::Transaction{i.action, ast::Transaction::Target{i.type, i.ident}});
		});
	};

	// Rule that matches a table create
	struct CreateTableTransaction {
		// Data acquired from the parse which needs to be rearranged to fit our data structures
		struct Intermediate {
			ast::Transaction::Action action;
			ast::Transaction::Target::Type type;
			std::string ident;
			std::optional<std::vector<Column>> columns;
		};

		// create table <id> [opt](<id> <type>, ...);
		static constexpr auto rule = KW::create + KW::table + identifier + dsl::opt(dsl::lit_c<'('> >> columnDeclarationList + dsl::lit_c<')'>) + stop;
		// Convert the parsed result into a Transcation smart pointer (unified type for all transactions)
		static constexpr auto value = lexy::construct<Intermediate> | lexy::callback<ast::Transaction::ptr>([](Intermediate&& i) {
			return std::make_unique<ast::CreateTableTransaction>(ast::CreateTableTransaction{i.action, ast::Transaction::Target{i.type, i.ident}, i.columns.value_or(std::vector<Column>{})});
		});
	};

	// Rule that matches a table query
	struct QueryTableTransaction {
		struct TableAlias {
			//id id?
			static constexpr auto rule = identifier + dsl::opt(identifier);
			static constexpr auto value = lexy::callback<sql::ast::QueryTableTransaction::TableAlias>([](auto&& table, std::optional<std::string>&& alias){
				return sql::ast::QueryTableTransaction::TableAlias{table, (alias.has_value() ? *alias : table)};
			});

			// A comma separated list of aliases
			struct List {
				static constexpr auto rule = dsl::list(dsl::p<TableAlias>, dsl::sep(dsl::comma));
				static constexpr auto value = lexy::as_list<std::vector<sql::ast::QueryTableTransaction::TableAlias>>;
			};
		};

		// Data acquired from the parse which needs to be rearranged to fit our data structures
		struct Intermediate {
			ast::Transaction::Action action;
			std::optional<std::vector<std::string>> columns;
			std::vector<sql::ast::QueryTableTransaction::TableAlias> tableAliases;
			std::optional<std::vector<WhereTransaction::Condition>> conditions;
		};

		// select */<id>,... from <aliasList> (where <conditions>)?;
		static constexpr auto rule = KW::select + (wildcard | identifierList) + KW::from + dsl::p<TableAlias::List> + dsl::opt(whereConditions) + stop;
		// Convert the parsed result into a Transcation smart pointer (unified type for all transactions)
		static constexpr auto value = lexy::construct<Intermediate> | lexy::callback<ast::Transaction::ptr>([](Intermediate&& i) -> ast::Transaction::ptr {
			using wc = sql::Wildcard<std::vector<std::string>>;
			wc columns = i.columns.has_value() ? (wc)i.columns.value() : (wc)std::nullopt;
			auto conditions = i.conditions.has_value() ? *i.conditions : std::vector<WhereTransaction::Condition>{};
			return std::make_unique<ast::QueryTableTransaction>(ast::QueryTableTransaction{i.action, ast::Transaction::Target{ast::Transaction::Target::Table, i.tableAliases.front().table}, conditions, i.tableAliases, columns});
		});
	};

	// Rule that matches a table insert
	struct InsertIntoTableTransaction {
		// Struct that parses a comma separated list of literals
		struct ValueList {
			static constexpr auto rule = dsl::list(literalVariant, dsl::sep(dsl::comma));
			static constexpr auto value = lexy::as_list<std::vector<Data::Variant>>;
		};
		// Data acquired from the parse which needs to be rearranged to fit our data structures
		struct Intermediate {
			ast::Transaction::Action action;
			std::string ident;
			std::vector<Data::Variant> values;
		};

		// insert into <id> values (<valueList>) ;
		static constexpr auto rule = KW::insert + KW::into + identifier + KW::values + dsl::lit_c<'('> + dsl::p<ValueList> + dsl::lit_c<')'> + stop;
		static constexpr auto value = lexy::construct<Intermediate> | lexy::callback<ast::Transaction::ptr>([](Intermediate&& i) {
			return std::make_unique<ast::InsertIntoTableTransaction>(ast::InsertIntoTableTransaction{i.action, ast::Transaction::Target{ast::Transaction::Target::Table, i.ident}, i.values});
		});
	};

	// Rule that matches a table alter
	struct AlterTableTransaction {
		// Data acquired from the parse which needs to be rearranged to fit our data structures
		struct Intermediate {
			ast::Transaction::Action action;
			ast::Transaction::Target::Type type;
			std::string ident;
			ast::Transaction::Action alterAction;
			Column alterTarget;
		};

		// alter table <id> add/remove/alter <id> [opt]<type>;
		static constexpr auto rule = KW::alter + KW::table + identifier + (((KW::add | KW::alter) >> columnDeclaration) | (KW::remove >> identifier)) + stop;
		// Convert the parsed result into a Transcation smart pointer (unified type for all transactions)
		static constexpr auto value = lexy::construct<Intermediate> | lexy::callback<ast::Transaction::ptr>([](Intermediate&& i) {
			return std::make_unique<ast::AlterTableTransaction>(ast::AlterTableTransaction{i.action, ast::Transaction::Target{i.type, i.ident}, i.alterAction, i.alterTarget});
		});
	};

	// Rule that matches a table value update
	struct UpdateTableTransaction {
		// Data acquired from the parse which needs to be rearranged to fit our data structures
		struct Intermediate {
			ast::Transaction::Action action;
			std::string table, column;
			Data::Variant value;
			std::optional<std::vector<WhereTransaction::Condition>> conditions;
		};

		// update <id> set <id> = <literal> where <conditions>;
		static constexpr auto rule = KW::update + identifier + KW::set + identifier + dsl::lit_c<'='> + literalVariant + whereConditions + stop;
		// Convert the parsed result into a Transcation smart pointer (unified type for all transactions)
		static constexpr auto value = lexy::construct<Intermediate> | lexy::callback<ast::Transaction::ptr>([](Intermediate&& i) -> ast::Transaction::ptr {
			auto conditions = i.conditions.has_value() ? *i.conditions : std::vector<WhereTransaction::Condition>{};
			return std::make_unique<ast::UpdateTableTransaction>(ast::UpdateTableTransaction{i.action, ast::Transaction::Target{ast::Transaction::Target::Table, i.table}, conditions, i.column, i.value});
		});
	};

	// Rule that matches a table value deletion
	struct DeleteFromTableTransaction {
		// Data acquired from the parse which needs to be rearranged to fit our data structures
		struct Intermediate {
			ast::Transaction::Action action;
			std::string table;
			std::optional<std::vector<WhereTransaction::Condition>> conditions;
		};

		// delete from <id> where <conditions>;
		static constexpr auto rule = KW::Delete + KW::from + identifier + whereConditions + stop;
		// Convert the parsed result into a Transcation smart pointer (unified type for all transactions)
		static constexpr auto value = lexy::construct<Intermediate> | lexy::callback<ast::Transaction::ptr>([](Intermediate&& i) -> ast::Transaction::ptr {
			auto conditions = i.conditions.has_value() ? *i.conditions : std::vector<WhereTransaction::Condition>{};
			return std::make_unique<ast::DeleteFromTableTransaction>(ast::DeleteFromTableTransaction{i.action, ast::Transaction::Target{ast::Transaction::Target::Table, i.table}, conditions});
		});
	};

	// Rule that matches any type of transaction and forwards the resulting smart pointer
	struct Transaction {
		static constexpr auto whitespace = wsc; // Automatic whitespace
		static constexpr auto rule = wss + (dsl::peek(KW::create + KW::database) >> dsl::p<DatabaseTransaction>
			| dsl::peek(KW::create + KW::table) >> dsl::p<CreateTableTransaction>
			| dsl::peek(KW::drop + KW::database) >> dsl::p<DatabaseTransaction>
			| dsl::peek(KW::drop + KW::table) >> dsl::p<DropTableTransaction>
			| dsl::peek(KW::use) >> dsl::p<UseDatabaseTransaction>
			| dsl::peek(KW::select) >> dsl::p<QueryTableTransaction>
			| dsl::peek(KW::alter) >> dsl::p<AlterTableTransaction>
			| dsl::peek(KW::insert) >> dsl::p<InsertIntoTableTransaction>
			| dsl::peek(KW::update) >> dsl::p<UpdateTableTransaction>
			| dsl::peek(KW::Delete) >> dsl::p<DeleteFromTableTransaction>);
		static constexpr auto value = lexy::forward<ast::Transaction::ptr>;
	};

} // sql::grammar


// Function which parses a SQL command
sql::ast::Transaction::ptr parseSQL(std::string in) {
	auto result = lexy::parse<sql::grammar::Transaction>(lexy::string_input<lexy::utf8_encoding>(in), lexy_ext::report_error);
	if(result.has_value())
		return std::move(const_cast<sql::ast::Transaction::ptr&>(result.value()));

	return {};
}