//
//  SSABuilder.cpp
//  uscc
//
//  Implements SSABuilder class
//  
//---------------------------------------------------------
//  Copyright (c) 2014, Sanjay Madhav
//  All rights reserved.
//
//  This file is distributed under the BSD license.
//  See LICENSE.TXT for details.
//---------------------------------------------------------

#include "SSABuilder.h"
#include "../parse/Symbols.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wconversion"
#include <llvm/IR/Value.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/CFG.h>
#include <llvm/IR/Constants.h>
#pragma clang diagnostic pop

#include <list>

using namespace uscc::opt;
using namespace uscc::parse;
using namespace llvm;

// Called when a new function is started to clear out all the data
void SSABuilder::reset()
{
	// PA5: Implement
	for (auto& def : mVarDefs)
	{
		delete def.second;
	}
	mVarDefs.clear();

	for (auto& phis : mIncompletePhis)
	{
		delete phis.second;
	}
	mIncompletePhis.clear();

	mSealedBlocks.clear();
}

// For a specific variable in a specific basic block, write its value
void SSABuilder::writeVariable(Identifier* var, BasicBlock* block, Value* value)
{
	// PA5: Implement
	SubMap *submap = mVarDefs[block];	
	(*submap)[var] = value;
}

// Read the value assigned to the variable in the requested basic block
// Will recursively search predecessor blocks if it was not written in this block
Value* SSABuilder::readVariable(Identifier* var, BasicBlock* block)
{
	// PA5: Implement
	if (mVarDefs[block]->find(var) != mVarDefs[block]->end())
	{
		SubMap *submap = mVarDefs[block];	
		return (*submap)[var];
	}
	else
	{
		return readVariableRecursive(var, block);
	}
}

// This is called to add a new block to the maps
void SSABuilder::addBlock(BasicBlock* block, bool isSealed /* = false */)
{
	// PA5: Implement
	mVarDefs[block] = new SubMap();
	mIncompletePhis[block] = new SubPHI();
	if (isSealed)
	{
		sealBlock(block);
	}
}

// This is called when a block is "sealed" which means it will not have any
// further predecessors added. It will complete any PHI nodes (if necessary)
void SSABuilder::sealBlock(llvm::BasicBlock* block)
{
	// PA5: Implement
	for (auto& phi : *mIncompletePhis[block])
	{
		addPhiOperands(phi.first, phi.second);
	}
	mSealedBlocks.insert(block);
}

// Recursively search predecessor blocks for a variable
Value* SSABuilder::readVariableRecursive(Identifier* var, BasicBlock* block)
{
	Value* retVal = nullptr;
	
	// PA5: Implement
	if (mSealedBlocks.find(block) == mSealedBlocks.end()) {
		if (block->getFirstNonPHI() != block->end()) {
			retVal = PHINode::Create(var->llvmType(), 0, "Phi", block->getFirstNonPHI());
		}
		else {
			retVal = PHINode::Create(var->llvmType(), 0, "Phi", block);
		}
		SubPHI *subphi = mIncompletePhis[block];
		(*subphi)[var] = dyn_cast<PHINode>(retVal);
	}
	else if (block->getSinglePredecessor() != nullptr) {
		retVal = readVariable(var, block->getSinglePredecessor());
	}
	else {
		if (block->getFirstNonPHI() != block->end()) {
			retVal = PHINode::Create(var->llvmType(), 0, "Phi", block->getFirstNonPHI());
		}
		else {
			retVal = PHINode::Create(var->llvmType(), 0, "Phi", block);
		}
		writeVariable(var, block, retVal);
		retVal = addPhiOperands(var, dyn_cast<PHINode>(retVal));
	}
	writeVariable(var, block, retVal);
	
	return retVal;
}

// Adds phi operands based on predecessors of the containing block
Value* SSABuilder::addPhiOperands(Identifier* var, PHINode* phi)
{
	// PA5: Implement
	for (auto pred = pred_begin(phi->getParent()); pred != pred_end(phi->getParent()); pred++)
	{
		Value* val = readVariable(var, *pred);
		phi->addIncoming(val, *pred);
	}
	return tryRemoveTrivialPhi(phi);
}

// Removes trivial phi nodes
Value* SSABuilder::tryRemoveTrivialPhi(llvm::PHINode* phi)
{
	Value* same = nullptr;
	
	// PA5: Implement
	for (int i = 0; i < phi->getNumIncomingValues(); i++) {
		Value *op = phi->getIncomingValue(i);
		if (op == same || op == phi) {
			continue;
		}
		else if (same != nullptr) {
			return phi;
		}
		same = op;
	}
	if (same == nullptr) {
		same = UndefValue::get(phi->getType());
	}

	phi->replaceAllUsesWith(same);
	for (auto& blocks : mVarDefs) {
		for (auto& defines : *blocks.second) {
			if (defines.second == phi) {
				defines.second = same;
			}
		}
	}
	phi->eraseFromParent();

	for (auto user = phi->user_begin(); user != phi->user_end(); user++) {
		if (PHINode *phi = dyn_cast_or_null<PHINode>(*user)) {
			tryRemoveTrivialPhi(phi);
		}
	}
	
	return same;
}
