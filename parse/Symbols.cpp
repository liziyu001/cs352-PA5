//
//  Symbols.cpp
//  uscc
//
//  Implements the symbol and string tables used for
//  semantic analysis.
//
//---------------------------------------------------------
//  Copyright (c) 2014, Sanjay Madhav
//  All rights reserved.
//
//  This file is distributed under the BSD license.
//  See LICENSE.TXT for details.
//---------------------------------------------------------

#include "Symbols.h"
#include "Emitter.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wconversion"
#include <llvm/IR/Type.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/IRBuilder.h>
#pragma clang diagnostic pop

#include <vector>
#include <algorithm>
#include <ostream>

using namespace uscc::parse;

llvm::Type* Identifier::llvmType(bool treatArrayAsPtr /* = true */) noexcept
{
	llvm::Type* type = nullptr;
	llvm::LLVMContext& context = llvm::getGlobalContext();
	switch (mType)
	{
		case Type::Char:
			type = llvm::Type::getInt8Ty(context);
			break;
		case Type::Int:
			type = llvm::Type::getInt32Ty(context);
			break;
		case Type::Void:
			type = llvm::Type::getVoidTy(context);
			break;
		case Type::CharArray:
			// Are we treating arrays as pointers?
			if (!treatArrayAsPtr)
			{
				type = llvm::ArrayType::get(llvm::Type::getInt8Ty(context),
											mArrayCount);
			}
			else
			{
				type = llvm::Type::getInt8PtrTy(context);
			}
			break;
		case Type::IntArray:
			// Are we treating arrays as pointers?
			if (!treatArrayAsPtr)
			{
				type = llvm::ArrayType::get(llvm::Type::getInt32Ty(context),
											mArrayCount);
			}
			else
			{
				type = llvm::Type::getInt32PtrTy(context);
			}
			break;
		case Type::Function:
			break;
	}
	
	return type;
}

llvm::Value* Identifier::readFrom(CodeContext& ctx) noexcept
{
	// PA5: Rewrite this entire function

	llvm::Value* retVal = nullptr;

	retVal = ctx.mSSA.readVariable(this, ctx.mBlock);
	// // Special case for arrays local to this function
	// if (isArray() && getArrayCount() != -1)
	// {
	// 	retVal = getAddress();
	// }
	// else
	// {
	// 	// PA3: Load from the memory address of this identifier
    //     llvm::IRBuilder<> builder(ctx.mBlock);
    //     retVal = builder.CreateLoad(getAddress(), getName());
	// }
	return retVal;
}

void Identifier::writeTo(CodeContext& ctx, llvm::Value* value) noexcept
{
	// PA5: Rewrite this entire function

	ctx.mSSA.writeVariable(this, ctx.mBlock, value);
	// Special case for arrays local to this function
	// if (isArray() && getArrayCount() != -1)
	// {
	// 	setAddress(value);
	// }
	// else
	// {
	// 	// PA3: Write to memory address of this identifier
    //     llvm::IRBuilder<> builder(ctx.mBlock);
    //     builder.CreateStore(value, getAddress());
	// }
}

SymbolTable::SymbolTable() noexcept
: mCurrScope(nullptr)
{
	// PA2: Implement
	mCurrScope = enterScope();
	auto id = createIdentifier("@@function");
	id->setType(Type::Function);
	id = createIdentifier("@@variable");
	id->setType(Type::Int);
	id = createIdentifier("printf");
	id->setType(Type::Function);
}

SymbolTable::~SymbolTable() noexcept
{
	// PA2: Implement
	delete mCurrScope;
}

// Returns true if this variable is already declared
// in this scope (ignoring parent scopes).
// Used to prevent redeclaration in the same scope,
// which is disallowed.
bool SymbolTable::isDeclaredInScope(const char* name) const noexcept
{
	// PA2: Implement
	return mCurrScope->searchInScope(name) != nullptr;
}

// Creates the requested identifier, and returns a pointer
// to it.
// NOTE: If the identifier already exists, nothing will happen.
// This means you should first check with isDeclaredInScope.
Identifier* SymbolTable::createIdentifier(const char* name)
{
	Identifier* ident = new Identifier(name);
	
	// PA2: Add to current scope table
	if (!isDeclaredInScope(name))	
		mCurrScope->addIdentifier(ident);

	return ident;
}

// Returns a pointer to the identifier, if it's found
// Otherwise returns nullptr
Identifier* SymbolTable::getIdentifier(const char* name)
{
	// PA2: Implement properly
	return mCurrScope->search(name);
}

// Enters a new scope, and returns a pointer to this scope table
SymbolTable::ScopeTable* SymbolTable::enterScope()
{
	// PA2: Implement
	mCurrScope = new ScopeTable(mCurrScope);
	return mCurrScope;
}

// Prints the symbol table to the specified stream
void SymbolTable::print(std::ostream& output) const noexcept
{
	output << "Symbols:\n";
	if (mCurrScope)
	{
		mCurrScope->print(output);
	}
}

// Exits the current scope and moves the current scope back to
// the previous scope table.
void SymbolTable::exitScope()
{
	// PA2: Implement
	mCurrScope = mCurrScope->getParent();
}

SymbolTable::ScopeTable::ScopeTable(ScopeTable* parent) noexcept
: mParent(parent)
{
	// PA2: Implement
	if (parent)
		parent->mChildren.emplace_back(this);
}

SymbolTable::ScopeTable::~ScopeTable() noexcept
{
	// PA2: Implement
	for (auto t : mChildren)
		delete t;
}

// Adds the requested identifier to the table
void SymbolTable::ScopeTable::addIdentifier(Identifier* ident)
{
	// PA2: Implement
	mSymbols.emplace(ident->getName(), ident);
}

// Searches this scope for an identifier with
// the requested name. Returns nullptr if not found.
Identifier* SymbolTable::ScopeTable::searchInScope(const char* name) noexcept
{
	// PA2: Implement
	auto search = mSymbols.find(name);
	if (search != mSymbols.end())
		return search->second;
	return nullptr;
}

// Searches this scope first, and if not found searches
// through parent scopes. Returns nullptr if not found.
Identifier* SymbolTable::ScopeTable::search(const char* name) noexcept
{
	// PA2: Implement
	auto ret = searchInScope(name);
	if (ret)
		return ret;
	if (mParent)
		return mParent->search(name);
	return nullptr;
}

void SymbolTable::ScopeTable::emitIR(CodeContext& ctx)
{
	// The ONLY thing we should alloca now are arrays of a specified size
	// First emit all the symbols in this scope
	for (auto sym : mSymbols)
	{
		Identifier* ident = sym.second;
		llvm::IRBuilder<> build(ctx.mBlock);

		llvm::Value* decl = nullptr;
		
		std::string name = ident->getName();
		
		// It's -1 if it's an array that's passed into a function,
		// in which case we don't allocate it
		if (ident->isArray() && ident->getArrayCount() != -1)
		{
			llvm::Type* type = ident->llvmType(false);
			// Note we pass in "nullptr" for the array size because that's
			// handled by the type
			decl = build.CreateAlloca(type, nullptr, name);
			llvm::cast<llvm::AllocaInst>(decl)->setAlignment(8);
			
			// Make a GEP here so we can access it later on without issue
			std::vector<llvm::Value*> gepIdx;
			gepIdx.push_back(ctx.mZero);
			gepIdx.push_back(ctx.mZero);
			
			decl = build.CreateInBoundsGEP(decl, gepIdx);
			
			// Now write this GEP and save it for this identifier
			ident->writeTo(ctx, decl);
		}
	}
	
	// Now emit all the variables in the child scope tables
	for (auto table : mChildren)
	{
		table->emitIR(ctx);
	}
}

// Prints the scope table to the specified stream
void SymbolTable::ScopeTable::print(std::ostream& output, int depth) const noexcept
{
	std::vector<Identifier*> idents;
	for (const auto& sym : mSymbols)
	{
		idents.push_back(sym.second);
	}

	std::sort(idents.begin(), idents.end(), [](Identifier* a, Identifier* b) {
		return a->getName() < b->getName();
	});

	for (const auto& ident : idents)
	{
		if (ident->getName()[0] == '@')
		{
			continue;
		}

		for (int i = 0; i < depth; i++)
		{
			output << "---";
		}

		switch (ident->getType())
		{
		case Type::Void:
			output << "void ";
			break;
		case Type::Int:
			output << "int ";
			break;
		case Type::Char:
			output << "char ";
			break;
		case Type::IntArray:
			output << "int[] ";
			break;
		case Type::CharArray:
			output << "char[] ";
			break;
		case Type::Function:
			output << "function ";
			break;
		default:
			output << "unknown ";
			break;
		}

		output << ident->getName();
		output << '\n';
	}

	for (const auto& child : mChildren)
	{
		child->print(output, depth + 1);
	}
}

StringTable::StringTable() noexcept
{
	
}

StringTable::~StringTable() noexcept
{
	for (auto i : mStrings)
	{
		delete i.second;
	}
}

// Looks up the requested string in the string table
// If it exists, returns the corresponding ConstStr
// Otherwise, constructs a new ConstStr and returns that
ConstStr* StringTable::getString(std::string& val) noexcept
{
	auto iter = mStrings.find(val);
	if (iter != mStrings.end())
	{
		return iter->second;
	}
	else
	{
		ConstStr* newStr = new ConstStr(val);
		mStrings.emplace(val, newStr);
		return newStr;
	}
}

void StringTable::emitIR(CodeContext& ctx) noexcept
{
	for (auto s : mStrings)
	{
		ConstStr* str = s.second;
		// Make the llvm value for this string
		llvm::Constant* strVal = llvm::ConstantDataArray::getString(ctx.mGlobal, str->mText);
		
		// Add this to the global table
		llvm::ArrayType* type = llvm::ArrayType::get(llvm::Type::getInt8Ty(ctx.mGlobal),
													 str->mText.size() + 1);
		
		
		llvm::GlobalValue* globVal =
			new llvm::GlobalVariable(*ctx.mModule, type, true,
									 llvm::GlobalValue::LinkageTypes::PrivateLinkage,
									 strVal, ".str");
		// This can be "unnamed" since the address location is not significant
		globVal->setUnnamedAddr(true);
		// Strings are 1-aligned
		//globVal->setAlignment(1);
		
		str->mValue = globVal;
	}
}
