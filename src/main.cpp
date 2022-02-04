#include <iostream>
#include "reader.hpp"

int main() {
	std::cout << "Hello World!" << std::endl;

	Reader r = Reader(/*enableHistory*/true)
		.setPrompt("> ");

	std::string s;
	while(((s = r.read())[0] != 'q'))
		std::cout << s << std::endl;
	// std::cout << r.historyPath.string() << std::endl;
}