// $Id: $                                                                                             

/*!
 * \file PHG4HitEval.h
 * \brief 
 * \author Jin Huang <jhuang@bnl.gov>
 * \version $Revision:   $
 * \date $Date: $
 */

#ifndef PHG4HITEVAL_H_
#define PHG4HITEVAL_H_

#include "PHG4Hitv1.h"

/*!
 * \brief PHG4HitEval for evaluating PHG4Hitv1 in CINT readable evaluation trees
 */
class PHG4HitEval : public PHG4Hitv1
{
public:
  PHG4HitEval();

  PHG4HitEval(const PHG4Hit& g4hit);

  virtual
  ~PHG4HitEval();

  virtual void Copy(PHG4Hit const &g4hit);

  float
  get_eion() const
  {
    return eion;
  }
  void
  set_eion(const float f)
  {
    eion = f;
  }

  int
  get_scint_id() const
  {
    return scint_id;
  }

  void
  set_scint_id(const int i)
  {
    scint_id = i;
  }

  float
  get_light_yield() const
  {
    return light_yield;
  }

  void
  set_light_yield(float lightYield)
  {
    light_yield = lightYield;
  }

  float
  get_path_length() const
  {
    return path_length;
  }

  void
  set_path_length(float pathLength)
  {
    path_length = pathLength;
  }

protected:

  float eion;

  int scint_id;

  //! a number proportional to the scintillation light yield.
  float light_yield;

  //! path length of the track to the hit
  float path_length;

ClassDef(PHG4HitEval,1)
};

#endif /* PHG4HITEVAL_H_ */
