#include <cctype>
#include <cstring>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#ifdef _WIN64 
#include <windows.h>
void* newCodePage() { return VirtualAlloc(0,1024,MEM_COMMIT,PAGE_EXECUTE_READWRITE); }
#define RETURN_REGISTERS {0xc1, 0xc2} // mov [rcx, rdx], eax
#else
#include <sys/mman.h>
void* newCodePage() { return mmap(0, 1024, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_ANON | MAP_PRIVATE, -1, 0); }
#define RETURN_REGISTERS {0xc7, 0xc6, 0xc2, 0xc1} // mov [edi/esi/edx/ecx], eax
#endif
#include <vector>

struct Source
{
	const char* content;
	std::size_t idx;
	std::string token;

	bool anythingBut(const std::string& str)
	{
		return nextToken() && token != str;
	}

	bool nextToken()
	{
		token = "";
		while (std::isspace(content[idx]))
			++idx;
		if (!content[idx])
			return false;
		if (std::isalpha(content[idx]))
			while (std::isalnum(content[idx]))
				token += content[idx++];
		else if (std::isdigit(content[idx]))
			while (std::isdigit(content[idx]))
				token += content[idx++];
		else switch(content[idx])
		{
			case '{':
			case '}':
			case '(':
			case ')':
			case ',':
			case '+':
			case '-':
			case '=':
			case ';': token = content[idx++]; break;
		}
		return true;
	}
};

struct Function;
struct Script
{
	Source source;
	std::map<std::string, std::unique_ptr<Function> > function;
};

struct Function
{
	char* code;
	unsigned int length;
	std::map<std::string, char> names;
	std::vector<std::string> arguments;
	Function()
		: length(9)
	{
//		code = static_cast<char*>(mmap(0, 1024, PROT_READ | PROT_WRITE | PROT_EXEC , MAP_ANON | MAP_PRIVATE, -1, 0));
		code = static_cast<char*>(newCodePage());
		for (unsigned int i=0; i<1024; ++i) code[i] = 0x90;
		code[0] = 0x53;	// push ebx
		code[1] = 0x55;	// push rbp
		code[2] = 0x48; // mov rbp,rsp
		code[3] = 0x89;
		code[4] = 0xe5;
		code[5] = 0x48;	// sub rsp, 252
		code[6] = 0x83;
		code[7] = 0xec;
		code[8] = 0x70;
		code[1017] = 0x48; // add rsp, 252
		code[1018] = 0x83;
		code[1019] = 0xc4;
		code[1020] = 0x70;
		code[1021] = 0x5d; // pop rbp
		code[1022] = 0x5b; // pop ebx
		code[1023] = 0xc3; // ret
	}
	void add(char c)
	{
		code[length++] = c;
	}
	void addInt(int i)
	{
		std::memcpy(code + length, &i, 4);
		length += 4;
	}
	void primaryExpression(Source& source)
	{
		int i=0;
		for (char c : source.token)
			i = std::isdigit(c) ? i*10+(c-'0') : -1;
		if (i >=0)
		{
			add(0xb8); // mov imm32, eax
			addInt(i);
		}
		else
		{
			add(0x8b); // mov
			for (std::size_t i=0; i<arguments.size(); ++i)
				if (source.token == arguments[i])
				{
					add(std::vector<unsigned char>RETURN_REGISTERS[i]);
					return;
				}
			add(0x45); // [ebp+imm8], eax
			add(names[source.token]);
		}
	}

	void termExpression(Source& source)
	{
		primaryExpression(source);
		source.nextToken();
		while (source.token == "+" || source.token == "-")
		{
			unsigned char op = source.token == "+" ? 0x01 : 0x29;
			source.nextToken();
			add(0x50); // push eax
			primaryExpression(source);
			add(0x5b); // pop ebx
			add(op); // add/sub eax, [eax,ebx]
			add(0xd8);
			if (op == 0x29)
			{
				add(0xf7); // neg eax
				add(0xd8);
			}
			source.nextToken();
		}
	}

	void expression(Source& source)
	{
		termExpression(source);
		while (source.token == "=")
		{
			source.nextToken();
			char disp = code[length-1];
			expression(source);
			add(0x89); // mov [ebp+imm8], eax
			add(0x45);
			add(disp);
		}
	}

	void statement(Source& source)
	{
		if (source.token == "{")
		{
			while (source.anythingBut("}"))
				statement(source);
		} else if (source.token == "return")
		{
			source.nextToken();
			expression(source);
			source.nextToken();
			add(0xe9); // jmp
			addInt(1013-length);
		} else if (source.token == "int")
		{
			source.nextToken();
			names[source.token] = -4*(names.size()+1);
			source.nextToken();
		} else if (source.token == "while")
		{
			source.nextToken();
			source.nextToken();
			expression(source);
			source.nextToken();
			unsigned int start = length;
			add(0x3d); // cmp eax, 0
			addInt(0);
			add(0x0f); // jne
			add(0x84);
			addInt(0);
			statement(source);
			add(0xe9); // jmp
			addInt(start-length - 7);
			int value = length-start - 11;
			memcpy(code + start + 7, &value, 4);
		}
		else
			expression(source);
	}

};

int main(int argc, char** argv)
{
	Script script;
	script.source.content = 
		"int fib(int param)"
		"{"
		" int a; int b; int c; int t;"
		" a = b = 1;"
		" c = param-2;"
		" while (c)"
		" {"
		"  t = a + b;"
		"  a = b;" 
		"  b = t;"
		"  c = c - 1;"
		" }"
		" return b;"
		"}";
	script.source.idx = 0;
	while (script.source.nextToken())
	{
		script.source.nextToken();
		script.function[script.source.token].reset(new Function);
		Function& function(*script.function[script.source.token]);
		script.source.nextToken();
		while (script.source.anythingBut(")"))
		{
			script.source.nextToken();
			function.arguments.push_back(script.source.token);
		}
		script.source.nextToken();
		function.statement(script.source);
	}

	typedef int(*func)(int);
	func f = reinterpret_cast<func>(script.function["fib"]->code);
	for (unsigned i=2; i<20; ++i)
		std::cout << "fib(" << i << ") = " << f(i) << std::endl;
	return 0;
}
