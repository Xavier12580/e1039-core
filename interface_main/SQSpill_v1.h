/*
 * SQSpill_v1.h
 *
 *  Created on: Oct 29, 2017
 *      Author: yuhw
 */

#ifndef _H_SQSpill_v1_H_
#define _H_SQSpill_v1_H_


#include <phool/PHObject.h>
#include <iostream>

#include "SQSpill.h"

class SQSpill_v1 : public SQSpill {

public:

  SQSpill_v1();
  virtual ~SQSpill_v1() {}

  // PHObject virtual overloads

  void         identify(std::ostream& os = std::cout) const;
  void         Reset() {*this = SQSpill_v1();}
  int          isValid() const;
  SQSpill*        Clone() const {return (new SQSpill_v1(*this));}

  virtual int          get_run_id() const                               {return _run_id;}
  virtual void         set_run_id(const int a)                          {_run_id = a;}

  virtual int          get_spill_id() const                               {return _spill_id;}
  virtual void         set_spill_id(const int a)                          {_spill_id = a;}

  virtual short        get_target_pos() const                               {return _target_pos;}
  virtual void         set_target_pos(const short a)                          {_target_pos = a;}


private:

  int _run_id;
  int _spill_id;
  short _target_pos;

  ClassDef(SQSpill_v1, 1);
};


#endif /* _H_SQSpill_v1_H_ */
