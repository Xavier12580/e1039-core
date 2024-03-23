#ifndef __PHPY8PARTICLETRIGGER_H__
#define __PHPY8PARTICLETRIGGER_H__

#include "PHPy8GenTrigger.h"
#include <string>

namespace Pythia8 {
  class Pythia;
};

class PHPy8ParticleTrigger : public PHPy8GenTrigger {

 public:

  PHPy8ParticleTrigger(const std::string &name = "PHPy8ParticleTrigger");
  virtual ~PHPy8ParticleTrigger();

  #ifndef __CINT__
  bool Apply(Pythia8::Pythia *pythia);
  #endif

  void AddParticles(std::string particles);
  void AddParticles(int particle);
  void AddParticles(std::vector<int> particles);

  void AddParents(std::string parents);
  void AddParents(int parent);
  void AddParents(std::vector<int> parents);

  void SetPtHigh(double pt);
  void SetPtLow(double pt);
  void SetPtHighLow(double ptHigh, double ptLow);
  
  void SetPHigh(double p);
  void SetPLow(double p);
  void SetPHighLow(double pHigh, double pLow);

  void SetEtaHigh(double eta);
  void SetEtaLow(double eta);
  void SetEtaHighLow(double etaHigh, double etaLow);

  void SetAbsEtaHigh(double eta);
  void SetAbsEtaLow(double eta);
  void SetAbsEtaHighLow(double etaHigh, double etaLow);

  void SetPxHigh(double pz);
  void SetPxLow(double pz);
  void SetPxHighLow(double pzHigh, double pzLow);

  void SetPyHigh(double pz);
  void SetPyLow(double pz);
  void SetPyHighLow(double pzHigh, double pzLow);

  void SetPzHigh(double pz);
  void SetPzLow(double pz);
  void SetPzHighLow(double pzHigh, double pzLow);

  void PrintConfig();

 private:

  std::vector<int> _theParents;
  std::vector<int> _theParticles;

  double _theEtaHigh, _theEtaLow;
  double _thePtHigh, _thePtLow;
  double _thePHigh, _thePLow;
  double _thePxHigh, _thePxLow;
  double _thePyHigh, _thePyLow;
  double _thePzHigh, _thePzLow;
  
  bool _doEtaHighCut, _doEtaLowCut, _doBothEtaCut;
  bool _doAbsEtaHighCut, _doAbsEtaLowCut, _doBothAbsEtaCut;
  bool _doPtHighCut, _doPtLowCut, _doBothPtCut;
  bool _doPHighCut, _doPLowCut, _doBothPCut;
  bool _doPxHighCut, _doPxLowCut, _doBothPxCut;
  bool _doPyHighCut, _doPyLowCut, _doBothPyCut;
  bool _doPzHighCut, _doPzLowCut, _doBothPzCut;
};

#endif	
