/*****************************************************************************
 * Project: RooFit                                                           *
 * Package: RooFitCore                                                       *
 * @(#)root/roofitcore:$Id$
 * Authors:                                                                  *
 *   WV, Wouter Verkerke, UC Santa Barbara, verkerke@slac.stanford.edu       *
 *   DK, David Kirkby,    UC Irvine,         dkirkby@uci.edu                 *
 *                                                                           *
 * Copyright (c) 2000-2005, Regents of the University of California          *
 *                          and Stanford University. All rights reserved.    *
 *                                                                           *
 * Redistribution and use in source and binary forms,                        *
 * with or without modification, are permitted according to the terms        *
 * listed in LICENSE (http://roofit.sourceforge.net/license.txt)             *
 *****************************************************************************/


/**
\file RooCmdConfig.cxx
\class RooCmdConfig
\ingroup Roofitcore

Class RooCmdConfig is a configurable parser for RooCmdArg named
arguments. It maps the contents of named arguments named to integers,
doubles, strings and TObjects that can be retrieved after processing
a set of RooCmdArgs. The parser also has options to enforce syntax
rules such as (conditionally) required arguments, mutually exclusive
arguments and dependencies between arguments
**/

#include "RooFit.h"

#include "RooCmdConfig.h"
#include "RooInt.h"
#include "RooDouble.h"
#include "RooArgSet.h"
#include "RooStringVar.h"
#include "RooTObjWrap.h"
#include "RooAbsData.h"
#include "RooMsgService.h"

#include "ROOT/StringUtils.hxx"

#include <iostream>


using namespace std;

ClassImp(RooCmdConfig);


////////////////////////////////////////////////////////////////////////////////
/// Constructor taking descriptive name of owner/user which
/// is used as prefix for any warning or error messages
/// generated by this parser

RooCmdConfig::RooCmdConfig(const char* methodName) :
  TObject(),
  _name(methodName)
{
  _iList.SetOwner() ;
  _dList.SetOwner() ;
  _sList.SetOwner() ;
  _cList.SetOwner() ;
  _oList.SetOwner() ;
  _rList.SetOwner() ;
  _fList.SetOwner() ;
  _mList.SetOwner() ;
  _yList.SetOwner() ;
  _pList.SetOwner() ;
}


namespace {

void cloneList(TList const& inList, TList & outList) {
  outList.SetOwner(true);
  std::unique_ptr<TIterator> iter{inList.MakeIterator()} ;
  while(auto elem = iter->Next()) {
    outList.Add(elem->Clone()) ;
  }
}

} // namespace


////////////////////////////////////////////////////////////////////////////////
/// Copy constructor

RooCmdConfig::RooCmdConfig(const RooCmdConfig& other)  : TObject(other)
{
  _name   = other._name ;
  _verbose = other._verbose ;
  _error = other._error ;
  _allowUndefined = other._allowUndefined ;

  cloneList(other._iList, _iList); // Integer list
  cloneList(other._dList, _dList); // Double list
  cloneList(other._sList, _sList); // String list
  cloneList(other._oList, _oList); // Object list
  cloneList(other._cList, _cList); // RooArgSet list

  cloneList(other._rList, _rList); // Required cmd list
  cloneList(other._fList, _fList); // Forbidden cmd list
  cloneList(other._mList, _mList); // Mutex cmd list 
  cloneList(other._yList, _yList); // Dependency cmd list
  cloneList(other._pList, _pList); // Processed cmd list 
}



////////////////////////////////////////////////////////////////////////////////
/// Return string with names of arguments that were required, but not
/// processed

std::string RooCmdConfig::missingArgs() const 
{
  std::string ret = "";

  bool first = true;
  std::unique_ptr<TIterator> iter{_rList.MakeIterator()};
  while(auto const& s = iter->Next()) {
    if (first) {
      first=kFALSE ;
    } else {
      ret += ", ";
    }
    ret += static_cast<TObjString*>(s)->String();
  }

  return ret;
}



////////////////////////////////////////////////////////////////////////////////
/// Define that processing argument name refArgName requires processing
/// of argument named neededArgName to successfully complete parsing

void RooCmdConfig::defineDependency(const char* refArgName, const char* neededArgName) 
{
  _yList.Add(new TNamed(refArgName,neededArgName)) ;
}



////////////////////////////////////////////////////////////////////////////////
/// Define integer property name 'name' mapped to integer in slot 'intNum' in RooCmdArg with name argName
/// Define default value for this int property to be defVal in case named argument is not processed

Bool_t RooCmdConfig::defineInt(const char* name, const char* argName, Int_t intNum, Int_t defVal)
{
  if (_iList.FindObject(name)) {
    coutE(InputArguments) << "RooCmdConfig::defintInt: name '" << name << "' already defined" << endl ;
    return kTRUE ;
  }

  RooInt* ri = new RooInt(defVal) ;
  ri->SetName(name) ;
  ri->SetTitle(argName) ;
  ri->SetUniqueID(intNum) ;
  
  _iList.Add(ri) ;
  return kFALSE ;
}



////////////////////////////////////////////////////////////////////////////////
/// Define Double_t property name 'name' mapped to Double_t in slot 'doubleNum' in RooCmdArg with name argName
/// Define default value for this Double_t property to be defVal in case named argument is not processed

Bool_t RooCmdConfig::defineDouble(const char* name, const char* argName, Int_t doubleNum, Double_t defVal) 
{
  if (_dList.FindObject(name)) {
    coutE(InputArguments) << "RooCmdConfig::defineDouble: name '" << name << "' already defined" << endl ;
    return kTRUE ;
  }

  RooDouble* rd = new RooDouble(defVal) ;
  rd->SetName(name) ;
  rd->SetTitle(argName) ;
  rd->SetUniqueID(doubleNum) ;
  
  _dList.Add(rd) ;
  return kFALSE ;
}



////////////////////////////////////////////////////////////////////////////////
/// Define Double_t property name 'name' mapped to Double_t in slot 'stringNum' in RooCmdArg with name argName
/// Define default value for this Double_t property to be defVal in case named argument is not processed
/// If appendMode is true, values found in multiple matching RooCmdArg arguments will be concatenated
/// in the output string. If it is false, only the value of the last processed instance is retained

Bool_t RooCmdConfig::defineString(const char* name, const char* argName, Int_t stringNum, const char* defVal, Bool_t appendMode) 
{
  if (_sList.FindObject(name)) {
    coutE(InputArguments) << "RooCmdConfig::defineString: name '" << name << "' already defined" << endl ;
    return kTRUE ;
  }

  RooStringVar* rs = new RooStringVar(name,argName,defVal,64000) ;
  if (appendMode) {
    rs->setAttribute("RooCmdConfig::AppendMode") ;
  }
  rs->SetUniqueID(stringNum) ;
  
  _sList.Add(rs) ;
  return kFALSE ;
}



////////////////////////////////////////////////////////////////////////////////
/// Define TObject property name 'name' mapped to object in slot 'setNum' in RooCmdArg with name argName
/// Define default value for this TObject property to be defVal in case named argument is not processed.
/// If isArray is true, an array of TObjects is harvested in case multiple matching named arguments are processed.
/// If isArray is false, only the TObject in the last processed named argument is retained

Bool_t RooCmdConfig::defineObject(const char* name, const char* argName, Int_t setNum, const TObject* defVal, Bool_t isArray) 
{

  if (_oList.FindObject(name)) {
    coutE(InputArguments) << "RooCmdConfig::defineObject: name '" << name << "' already defined" << endl ;
    return kTRUE ;
  }

  RooTObjWrap* os = new RooTObjWrap((TObject*)defVal,isArray) ;
  os->SetName(name) ;
  os->SetTitle(argName) ;
  os->SetUniqueID(setNum) ;
  
  _oList.Add(os) ;
  return kFALSE ;
}



////////////////////////////////////////////////////////////////////////////////
/// Define TObject property name 'name' mapped to object in slot 'setNum' in RooCmdArg with name argName
/// Define default value for this TObject property to be defVal in case named argument is not processed.
/// If isArray is true, an array of TObjects is harvested in case multiple matching named arguments are processed.
/// If isArray is false, only the TObject in the last processed named argument is retained

Bool_t RooCmdConfig::defineSet(const char* name, const char* argName, Int_t setNum, const RooArgSet* defVal) 
{

  if (_cList.FindObject(name)) {
    coutE(InputArguments) << "RooCmdConfig::defineObject: name '" << name << "' already defined" << endl ;
    return kTRUE ;
  }

  RooTObjWrap* cs = new RooTObjWrap((TObject*)defVal) ;
  cs->SetName(name) ;
  cs->SetTitle(argName) ;
  cs->SetUniqueID(setNum) ;
  
  _cList.Add(cs) ;
  return kFALSE ;
}



////////////////////////////////////////////////////////////////////////////////
/// Print configuration of parser

void RooCmdConfig::print() const
{
  // Find registered integer fields for this opcode 
  std::unique_ptr<TIterator> iter{_iList.MakeIterator()};
  while(auto const& ri = iter->Next()) {
    cout << ri->GetName() << "[Int_t] = " << static_cast<RooInt const&>(*ri) << endl ;
  }

  // Find registered double fields for this opcode 
  iter.reset(_dList.MakeIterator());
  while(auto const& rd = iter->Next()) {
    cout << rd->GetName() << "[Double_t] = " << static_cast<RooDouble const&>(*rd) << endl ;
  }

  // Find registered string fields for this opcode 
  iter.reset(_sList.MakeIterator());
  while(auto const& rs = iter->Next()) {
    cout << rs->GetName() << "[string] = \"" << static_cast<RooStringVar const&>(*rs).getVal() << "\"" << endl ;
  }

  // Find registered argset fields for this opcode 
  iter.reset(_oList.MakeIterator());
  while(auto const& ro = iter->Next()) {
    cout << ro->GetName() << "[TObject] = " ; 
    auto const * obj = static_cast<RooTObjWrap const&>(ro).obj();
    if (obj) {
      cout << obj->GetName() << endl ;
    } else {

      cout << "(null)" << endl ;
    }
  }
}



////////////////////////////////////////////////////////////////////////////////
/// Process given list with RooCmdArgs

Bool_t RooCmdConfig::process(const RooLinkedList& argList) 
{
  Bool_t ret(kFALSE) ;
  for(auto * arg : static_range_cast<RooCmdArg*>(argList)) {
    ret |= process(*arg) ;
  }
  return ret ;
}



////////////////////////////////////////////////////////////////////////////////
/// Process given RooCmdArg

Bool_t RooCmdConfig::process(const RooCmdArg& arg) 
{
  // Retrive command code
  const char* opc = arg.opcode() ;

  // Ignore empty commands
  if (!opc) return kFALSE ;

  // Check if not forbidden
  if (_fList.FindObject(opc)) {
    coutE(InputArguments) << _name << " ERROR: argument " << opc << " not allowed in this context" << endl ;
    _error = kTRUE ;
    return kTRUE ;
  }

  // Check if this code generates any dependencies
  TObject* dep = _yList.FindObject(opc) ;
  if (dep) {
    // Dependent command found, add to required list if not already processed
    if (!_pList.FindObject(dep->GetTitle())) {
      _rList.Add(new TObjString(dep->GetTitle())) ;
      if (_verbose) {
	cout << "RooCmdConfig::process: " << opc << " has unprocessed dependent " << dep->GetTitle() 
	     << ", adding to required list" << endl ;
      }
    } else {
      if (_verbose) {
	cout << "RooCmdConfig::process: " << opc << " dependent " << dep->GetTitle() << " is already processed" << endl ;
      }
    }
  }

  // Check for mutexes
  TObject * mutex = _mList.FindObject(opc) ;
  if (mutex) {
    if (_verbose) {
      cout << "RooCmdConfig::process: " << opc << " excludes " << mutex->GetTitle() 
	   << ", adding to forbidden list" << endl ;
    }    
    _fList.Add(new TObjString(mutex->GetTitle())) ;
  }


  Bool_t anyField(kFALSE) ;

  // Find registered integer fields for this opcode 
  std::unique_ptr<TIterator> iter{_iList.MakeIterator()};
  while(auto const& elem = iter->Next()) {
    auto ri = static_cast<RooInt*>(elem);
    if (!TString(opc).CompareTo(ri->GetTitle())) {
      *ri = arg.getInt(ri->GetUniqueID()) ;
      anyField = kTRUE ;
      if (_verbose) {
	cout << "RooCmdConfig::process " << ri->GetName() << "[Int_t]" << " set to " << *ri << endl ;
      }
    }
  }

  // Find registered double fields for this opcode 
  iter.reset(_dList.MakeIterator());
  while(auto const& elem = iter->Next()) {
    auto rd = static_cast<RooDouble*>(elem);
    if (!TString(opc).CompareTo(rd->GetTitle())) {
      *rd = arg.getDouble(rd->GetUniqueID()) ;
      anyField = kTRUE ;
      if (_verbose) {
	cout << "RooCmdConfig::process " << rd->GetName() << "[Double_t]" << " set to " << *rd << endl ;
      }
    }
  }

  // Find registered string fields for this opcode 
  iter.reset(_sList.MakeIterator());
  while(auto const& elem = iter->Next()) {
    auto rs = static_cast<RooStringVar*>(elem);
    if (!TString(opc).CompareTo(rs->GetTitle())) {
      
      const char* oldStr = rs->getVal() ;

      if (oldStr && strlen(oldStr)>0 && rs->getAttribute("RooCmdConfig::AppendMode")) {
	rs->setVal(Form("%s,%s",rs->getVal(),arg.getString(rs->GetUniqueID()))) ;
      } else {
	rs->setVal(arg.getString(rs->GetUniqueID())) ;
      }
      anyField = kTRUE ;
      if (_verbose) {
	cout << "RooCmdConfig::process " << rs->GetName() << "[string]" << " set to " << rs->getVal() << endl ;
      }
    }
  }

  // Find registered TObject fields for this opcode 
  iter.reset(_oList.MakeIterator());
  while(auto const& elem = iter->Next()) {
    auto os = static_cast<RooTObjWrap*>(elem);
    if (!TString(opc).CompareTo(os->GetTitle())) {
      os->setObj((TObject*)arg.getObject(os->GetUniqueID())) ;
      anyField = kTRUE ;
      if (_verbose) {
	cout << "RooCmdConfig::process " << os->GetName() << "[TObject]" << " set to " ;
	if (os->obj()) {
	  cout << os->obj()->GetName() << endl ;
	} else {
	  cout << "(null)" << endl ;
	}
      }
    }
  }

  // Find registered RooArgSet fields for this opcode 
  iter.reset(_cList.MakeIterator());
  while(auto const& elem = iter->Next()) {
    auto cs = static_cast<RooTObjWrap*>(elem);
    if (!TString(opc).CompareTo(cs->GetTitle())) {
      cs->setObj((TObject*)arg.getSet(cs->GetUniqueID())) ;
      anyField = kTRUE ;
      if (_verbose) {
	cout << "RooCmdConfig::process " << cs->GetName() << "[RooArgSet]" << " set to " ;
	if (cs->obj()) {
	  cout << cs->obj()->GetName() << endl ;
	} else {
	  cout << "(null)" << endl ;
	}
      }
    }
  }

  Bool_t multiArg = !TString("MultiArg").CompareTo(opc) ;

  if (!anyField && !_allowUndefined && !multiArg) {
    coutE(InputArguments) << _name << " ERROR: unrecognized command: " << opc << endl ;
  }


  // Remove command from required-args list (if it was there)
  TObject* obj;
  while ( (obj = _rList.FindObject(opc)) ) {
    _rList.Remove(obj);
  }

  // Add command the processed list
  TNamed *pcmd = new TNamed(opc,opc) ;
  _pList.Add(pcmd) ;

  Bool_t depRet = kFALSE ;
  if (arg.procSubArgs()) {
    for (Int_t ia=0 ; ia<arg.subArgs().GetSize() ; ia++) {
      RooCmdArg* subArg = static_cast<RooCmdArg*>(arg.subArgs().At(ia)) ;
      if (strlen(subArg->GetName())>0) {
	RooCmdArg subArgCopy(*subArg) ;
	if (arg.prefixSubArgs()) {
	  subArgCopy.SetName(Form("%s::%s",arg.GetName(),subArg->GetName())) ;
	}
	depRet |= process(subArgCopy) ;
      }
    }
  }

  return ((anyField||_allowUndefined)?kFALSE:kTRUE)||depRet ;
}
  


////////////////////////////////////////////////////////////////////////////////
/// Return true if RooCmdArg with name 'cmdName' has been processed

Bool_t RooCmdConfig::hasProcessed(const char* cmdName) const 
{
  return _pList.FindObject(cmdName) ? kTRUE : kFALSE ;
}



////////////////////////////////////////////////////////////////////////////////
/// Return integer property registered with name 'name'. If no
/// property is registered, return defVal

Int_t RooCmdConfig::getInt(const char* name, Int_t defVal) 
{
  RooInt* ri = (RooInt*) _iList.FindObject(name) ;
  return ri ? (Int_t)(*ri) : defVal ;
}



////////////////////////////////////////////////////////////////////////////////
/// Return Double_t property registered with name 'name'. If no
/// property is registered, return defVal

Double_t RooCmdConfig::getDouble(const char* name, Double_t defVal) 
{
  RooDouble* rd = (RooDouble*) _dList.FindObject(name) ;
  return rd ? (Double_t)(*rd) : defVal ;
}



////////////////////////////////////////////////////////////////////////////////
/// Return string property registered with name 'name'. If no
/// property is registered, return defVal. If convEmptyToNull
/// is true, empty string will be returned as null pointers

const char* RooCmdConfig::getString(const char* name, const char* defVal, Bool_t convEmptyToNull) 
{
  RooStringVar* rs = (RooStringVar*) _sList.FindObject(name) ;
  return rs ? ((convEmptyToNull && strlen(rs->getVal())==0) ? 0 : ((const char*)rs->getVal()) ) : defVal ;
}



////////////////////////////////////////////////////////////////////////////////
/// Return TObject property registered with name 'name'. If no
/// property is registered, return defVal

TObject* RooCmdConfig::getObject(const char* name, TObject* defVal) 
{
  RooTObjWrap* ro = (RooTObjWrap*) _oList.FindObject(name) ;
  return ro ? ro->obj() : defVal ;
}


////////////////////////////////////////////////////////////////////////////////
/// Return RooArgSet property registered with name 'name'. If no
/// property is registered, return defVal

RooArgSet* RooCmdConfig::getSet(const char* name, RooArgSet* defVal) 
{
  RooTObjWrap* ro = (RooTObjWrap*) _cList.FindObject(name) ;
  return ro ? ((RooArgSet*)ro->obj()) : defVal ;
}



////////////////////////////////////////////////////////////////////////////////
/// Return list of objects registered with name 'name'

const RooLinkedList& RooCmdConfig::getObjectList(const char* name) 
{
  const static RooLinkedList defaultDummy ;
  RooTObjWrap* ro = (RooTObjWrap*) _oList.FindObject(name) ;
  return ro ? ro->objList() : defaultDummy ;
}



////////////////////////////////////////////////////////////////////////////////
/// Return true of parsing was successful

Bool_t RooCmdConfig::ok(Bool_t verbose) const 
{ 
  if (_rList.GetSize()==0 && !_error) return kTRUE ;

  if (verbose) {
    std::string margs = missingArgs() ;
    if (!margs.empty()) {
      coutE(InputArguments) << _name << " ERROR: missing arguments: " << margs << endl ;
    } else {
      coutE(InputArguments) << _name << " ERROR: illegal combination of arguments and/or missing arguments" << endl ;
    }
  }
  return kFALSE ;
}



////////////////////////////////////////////////////////////////////////////////
/// Utility function that strips command names listed (comma separated) in cmdsToPurge from cmdList

void RooCmdConfig::stripCmdList(RooLinkedList& cmdList, const char* cmdsToPurge) const
{
  // Sanity check
  if (!cmdsToPurge) return ;
  
  // Copy command list for parsing
  for(auto const& name : ROOT::Split(cmdsToPurge, ",")) {
    if (TObject* cmd = cmdList.FindObject(name.c_str())) {
      cmdList.Remove(cmd);
    }
  }

}



////////////////////////////////////////////////////////////////////////////////
/// Utility function to filter commands listed in cmdNameList from cmdInList. Filtered arguments are put in the returned list.
/// If removeFromInList is true then these commands are removed from the input list

RooLinkedList RooCmdConfig::filterCmdList(RooLinkedList& cmdInList, const char* cmdNameList, bool removeFromInList) const
{
  RooLinkedList filterList ;
  if (!cmdNameList) return filterList ;

  // Copy command list for parsing
  for(auto const& name : ROOT::Split(cmdNameList, ",")) {
    if (TObject* cmd = cmdInList.FindObject(name.c_str())) {
      if (removeFromInList) {
        cmdInList.Remove(cmd) ;
      }
      filterList.Add(cmd) ;
    }
  }
  return filterList ;  
}



////////////////////////////////////////////////////////////////////////////////
/// Find a given double in a list of RooCmdArg.
/// Should only be used to initialise base classes in constructors.
double RooCmdConfig::decodeDoubleOnTheFly(const char* callerID, const char* cmdArgName, int idx, double defVal,
    std::initializer_list<std::reference_wrapper<const RooCmdArg>> args) {
  RooCmdConfig pc(callerID);
  pc.allowUndefined();
  pc.defineDouble("theDouble", cmdArgName, idx, defVal);
  pc.process(args.begin(), args.end());
  return pc.getDouble("theDouble");
}
