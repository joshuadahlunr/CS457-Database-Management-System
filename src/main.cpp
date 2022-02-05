#include <iostream>
#include "reader.hpp"

#include "SQLparser.hpp"

int main() {
	std::cout << "Hello World!" << std::endl;

	// Reader r = Reader(/*enableHistory*/true)
	// 	.setPrompt("> ");

	// std::string s;
	// while(((s = r.read())[0] != 'q'))
	// 	std::cout << s << std::endl;


	auto r = parseSQL("USE BOB;");
	std::cout << r->action << " - " << r->target.type << " - " << r->target.name << std::endl;
}