#ifndef PHG4VConeRegionSteppingAction_h
#define PHG4VConeRegionSteppingAction_h

#include <Geant4/G4UserSteppingAction.hh>


class PHCompositeNode;
class PHG4ConeDetector;
class PHG4Hit;
class PHG4HitContainer;

class PHG4ConeRegionSteppingAction : public G4UserSteppingAction
{

  public:

  //! constructor
  PHG4ConeRegionSteppingAction( PHG4ConeDetector* );

  //! destroctor
  virtual ~PHG4ConeRegionSteppingAction()
  {}

  //! stepping action
  virtual void UserSteppingAction(const G4Step*);

  //! reimplemented from base class
  virtual void SetInterfacePointers( PHCompositeNode* );

  private:

  //! pointer to the detector
  PHG4ConeDetector* detector_;

  //! pointer to hit container
  PHG4HitContainer * hits_;
  PHG4Hit *hit;
};


#endif //__G4PHPHYTHIAREADER_H__
