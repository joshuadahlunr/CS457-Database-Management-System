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


	auto r = parseSQL("create table BOB (a1 char(10));");
	// auto r = parseSQL("create database bob;");
	std::cout << r->action << " - " << r->target.type << " - " << r->target.name << std::endl;
	for(sql::Column& c: ((sql::ast::CreateTableTransaction*) r.get())->columns)
		std::cout << c.name << " - " << c.type.type << " - " << c.type.size << std::endl;
	// if(((sql::ast::QueryTableTransaction*) r.get())->columns.all())
	// 	std::cout << "ALL" << std::endl;
	// else for(std::string& c: *((sql::ast::QueryTableTransaction*) r.get())->columns)
	// 	std::cout << c << std::endl;
	// sql::ast::AlterTableTransaction& at = *((sql::ast::AlterTableTransaction*) r.get());
	// std::cout << at.alterAction << " - " << at.alterTarget.name << " - " << at.alterTarget.type.type << std::endl;
}