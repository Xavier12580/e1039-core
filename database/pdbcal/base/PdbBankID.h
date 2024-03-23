//-----------------------------------------------------------------------------
//
//  The pdbcal package
//  Copyright (C) PHENIX collaboration, 1999
//
//  Declaration of class PdbBankID
//
//  Purpose: id number for a bank, derived from a string
//
//  Description: The string should follow the ONCS naming convention
//
//  Author: Matthias Messer
//-----------------------------------------------------------------------------
#ifndef PDBBANKID_HH__
#define PDBBANKID_HH__

#include <TObject.h>

class PdbBankID : public TObject {
public:
   PdbBankID(const int val = 0);
   virtual ~PdbBankID(){}

   void print() const;

   int  getInternalValue() const { return bankID; }
   void setInternalValue(const int val) { bankID = val; }
   
   friend int operator == (const PdbBankID &, const PdbBankID &);
   
private:
  int bankID;

  ClassDef(PdbBankID, 1)

};

#endif /* PDBBANKID_HH__ */
