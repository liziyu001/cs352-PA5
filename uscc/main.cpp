//
//  main.cpp
//  uscc
//
//  Main entry point for uscc.
//  This driver processes command-line parameters
//  and starts the compilation proces.
//
//---------------------------------------------------------
//  Copyright (c) 2014, Sanjay Madhav
//  All rights reserved.
//
//  This file is distributed under the BSD license.
//  See LICENSE.TXT for details.
//---------------------------------------------------------

#include "../parse/Parse.h"
#include "../parse/ParseExcept.h"
#include "../parse/Emitter.h"
#include <iostream>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wconversion"
#pragma clang diagnostic ignored "-Wunused"
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#include "ezOptionParser.hpp"
#pragma clang diagnostic pop
#pragma GCC diagnostic pop

using namespace uscc;
extern bool enableLiveness;

int main(int argc, const char * argv[])
{
	ez::ezOptionParser opt;
	opt.doublespace = 1;
	opt.overview = "University Simple C Compiler v0.5";
	opt.syntax = "uscc [OPTIONS] <input>";
	
	opt.add("", false, 0, 0,
			"Display this message.",
			"-h", "--help");
	opt.add("", false, 0, 0,
			"Output parse AST to stdout, and do not proceed to further compilation steps. "
			"(Unless -b or -s is also specified.)",
			"-a", "--print-ast");
	opt.add("", false, 0, 0,
			"(DEFAULT) Generates LLVM bitcode file."
			" This is done by default if"
			" -a or -s is not specified.\n\nTo force bitcode to be written even if -a or -s are"
			" set, you can specify -b, as well.",
			"-b", "--bitcode");
	opt.add("", false, 0, 0,
			"Output symbol table to stdout.",
			"-l", "--print-symbols");
	opt.add("", false, 0, 0,
			"Output LLVM IR to stdout.",
			"-p", "--print-bc");
	opt.add("", false, 0, 0,
			"Enable optimization passes.",
			"-O");
	opt.add("", false, 0, 0,
			"Generate an x86 assembly file from the LLVM IR generated by uscc."
			" No optimization is performed."
			"\n\nThis is provided for convenience in case LLVM developer tools (specifically llc)"
			" are not installed. GCC or clang can turn this assembly file into an executable.",
			"-s", "--assembly");
	opt.add("4", false, 1, 0, "Specify number of colors for register graph coloring", "--num-colors");
	opt.add("", false, 1, 0,
			"Specify output file. This is ignored if -b and -s are specified simultaneously.",
			"-o", "--output");

    opt.add("", false, 0, 0, "Enable liveness analysis",
            "-liveness");
    opt.add("", false, 0, 0, "Enable Dead Code Elimination",
            "-dce");

	opt.parse(argc, argv);
	if (opt.isSet("-h"))
	{
		std::string usage;
		opt.getUsage(usage);
		std::cout << usage;
		return 0;
	}
	
	if (opt.lastArgs.size() < 1)
	{
		std::cerr << "uscc: error: No input file specified." << std::endl;
		return 1;
	}
	if (opt.lastArgs.size() > 1)
	{
		std::cerr << "uscc: error: Only a single input file is supported." << std::endl;
		return 1;
	}
	
	const char* fileName = opt.lastArgs[0]->c_str();
	std::ostream* astStream = nullptr;
	bool outputSymbols = false;
	if (opt.isSet("-a"))
	{
		astStream = &std::cout;
	}

	if (opt.isSet("-l"))
	{
		outputSymbols = true;
	}
	
	try
	{
		parse::Parser parser(fileName, &std::cerr, astStream, outputSymbols);
		
		if (!parser.IsValid())
		{
			std::cerr << parser.GetNumErrors() << " Error(s)" << std::endl;
			return 1;
		}
		
		// If we set -a, we don't continue to later steps
		if (opt.isSet("-a") &&
			!opt.isSet("-b") && !opt.isSet("-s") && !opt.isSet("-p"))
		{
			return 0;
		}
		
		// Now emit LLVM bitcode
		parse::Emitter emit(parser);

        // Perform dead code elimination that calls liveness analysis.
        enableLiveness = opt.isSet("-liveness");
        if (enableLiveness)
        {
            emit.doLiveness();
            return 0;
        }
        else if (opt.isSet("-dce"))
        {
            emit.registerAnalysis();
            emit.doDCE();
        }

		// Check if we should run optimization passes
		if (opt.isSet("-O"))
		{
			emit.optimize();
		}
		
		bool shouldEmitBC = true;
		if (opt.isSet("-s") && !opt.isSet("-b"))
		{
			shouldEmitBC = false;
		}
		
		// Print the human readable bitcode to stdout
		if (opt.isSet("-p"))
		{
			emit.print();
		}
		
		// Before we write anything, verify the IR doesn't have major errors
		if (!emit.verify())
		{
			std::cerr << std::endl;
			std::cerr << "uscc: error: Emitted bad IR. Compilation halted." << std::endl;
			return 1;
		}
		
		// Write the bitcode file
		if (shouldEmitBC)
		{
			std::string bcFile;
			// If output file not specified, default is
			// input file with the extension replaced with .bc
			if (!opt.isSet("-o") || opt.isSet("-s"))
			{
				bcFile = fileName;
				size_t extLoc = bcFile.find_last_of(".");
				if (extLoc != std::string::npos)
				{
					// Strip the last extension
					bcFile = bcFile.substr(0, extLoc);
				}
				bcFile += ".bc";
			}
			else
			{
				ez::OptionGroup* params = opt.get("-o");
				params->getString(bcFile);
			}
			
			emit.writeBitcode(bcFile.c_str());
		}
		
		// Functionality removed because it doesn't work with LLVM 3.5.0
		// Write the assembly file
		/*if (opt.isSet("-s"))
		{
			std::string asmFile;
			// If output file not specified, default is
			// input file with the extension replaced with .bc
			if (!opt.isSet("-o") || opt.isSet("-b"))
			{
				asmFile = fileName;
				size_t extLoc = asmFile.find_last_of(".");
				if (extLoc != std::string::npos)
				{
					// Strip the last extension
					asmFile = asmFile.substr(0, extLoc);
				}
				asmFile += ".s";
			}
			else
			{
				ez::OptionGroup* params = opt.get("-o");
				params->getString(asmFile);
			}
			
			if (!emit.writeAsm(asmFile.c_str()))
			{
				std::cerr << "uscc: error: Unable to emit assembly. Compilation halted." << std::endl;
			}
		}*/
	}
	catch (parse::FileNotFound& fe)
	{
		std::cerr << "uscc: error: Input file " << fileName << " not found." << std::endl;
	}
	catch (parse::ParseExcept& e)
	{
		std::cerr << "uscc: error: Critical error. Compilation halted." << std::endl;
		return 1;
	}
	
	return 0;
}

