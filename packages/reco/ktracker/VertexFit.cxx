/*
VertexFit.cxx

Implementation of the VertexFit, in the beginning only a old-fashioned primary vertex finder
is implemented

Author: Kun Liu, liuk@fnal.gov
Created: 2-8-2012
*/

#include "VertexFit.h"
#include "KalmanTrack.h"
#include "KalmanFilter.h"
#include "GenFitExtrapolator.h"

#include <fun4all/Fun4AllReturnCodes.h>
#include <fun4all/PHTFileServer.h>
#include <phool/PHNodeIterator.h>
#include <phool/PHIODataNode.h>
#include <phool/getClass.h>
#include <phool/recoConsts.h>
#include <phfield/PHFieldConfig_v3.h>
#include <phfield/PHFieldUtility.h>
#include <phgeom/PHGeomUtility.h>

#include <TMatrixD.h>

#include <iostream>
#include <cmath>
#include <memory>
#include <fstream>

//#define _DEBUG_ON

using namespace std;

namespace 
{
    //static flag to indicate the initialized has been done
    static bool inited = false;

	//static flag of kmag strength
	static double FMAGSTR;
	static double KMAGSTR;

    //Beam position and shape
    static double X_BEAM;
    static double Y_BEAM;
    static double SIGX_BEAM;
    static double SIGY_BEAM;

    //Simple swimming settings 
    static int NSTEPS_TARGET;
    static int NSTEPS_SHIELDING;
    static int NSTEPS_FMAG;

    //Geometric constants
    static double Z_TARGET;
    static double Z_DUMP;
    static double Z_UPSTREAM;
    static double Z_DOWNSTREAM;

    //initialize global variables
    void initGlobalVariables()
    {
        if(!inited) 
        {
            inited = true;

            recoConsts* rc = recoConsts::instance();
            FMAGSTR = rc->get_DoubleFlag("FMAGSTR");
            KMAGSTR = rc->get_DoubleFlag("KMAGSTR");
            
            X_BEAM = rc->get_DoubleFlag("X_BEAM");
            Y_BEAM = rc->get_DoubleFlag("Y_BEAM");
            SIGX_BEAM = rc->get_DoubleFlag("SIGX_BEAM");
            SIGY_BEAM = rc->get_DoubleFlag("SIGY_BEAM");

            NSTEPS_TARGET = rc->get_IntFlag("NSTEPS_TARGET");
            NSTEPS_SHIELDING = rc->get_IntFlag("NSTEPS_SHIELDING");
            NSTEPS_FMAG = rc->get_IntFlag("NSTEPS_FMAG");

            Z_TARGET = rc->get_DoubleFlag("Z_TARGET");
            Z_DUMP = rc->get_DoubleFlag("Z_DUMP");
            Z_UPSTREAM = rc->get_DoubleFlag("Z_UPSTREAM");
            Z_DOWNSTREAM = rc->get_DoubleFlag("Z_DOWNSTREAM");
        }
    }
}

VertexFit::VertexFit(const std::string& name) :
    SubsysReco(name)
{
  initGlobalVariables();
  
  ///In construction, initialize the projector for the vertex node
  TMatrixD m(2, 1), cov(2, 2), proj(2, 5);
  m[0][0] = X_BEAM;
  m[1][0] = Y_BEAM;

  cov.Zero();
  cov[0][0] = SIGX_BEAM*SIGX_BEAM;
  cov[1][1] = SIGY_BEAM*SIGY_BEAM;

  proj.Zero();
  proj[0][3] = 1.;
  proj[1][4] = 1.;

  _node_vertex.getMeasurement().ResizeTo(2, 1);
  _node_vertex.getMeasurementCov().ResizeTo(2, 2);
  _node_vertex.getProjector().ResizeTo(2, 5);

  _node_vertex.getMeasurement() = m;
  _node_vertex.getMeasurementCov() = cov;
  _node_vertex.getProjector() = proj;

  _max_iteration = 200;
  _tolerance = .05;

  ///disable target optimization by default
  optimize = false;
  //disable targert center fit by default
  fit_target_center = false;

  ///disable evaluation by default
  evalFileName = "vertex_eval.root";
  evalFile = nullptr;
  evalTree = nullptr;

}

VertexFit::~VertexFit()
{
    if(evalFile != nullptr)
    {
        evalFile->cd();
        evalTree->Write();
        evalFile->Close();
    }
}

int VertexFit::Init(PHCompositeNode* topNode) {

  this->bookEvaluation(evalFileName.c_str());

  return Fun4AllReturnCodes::EVENT_OK;
}

int VertexFit::InitRun(PHCompositeNode* topNode) {

  FMAGSTR = recoConsts::instance()->get_DoubleFlag("FMAGSTR");
  KMAGSTR = recoConsts::instance()->get_DoubleFlag("KMAGSTR");

  int ret = MakeNodes(topNode);
  if(ret != Fun4AllReturnCodes::EVENT_OK) return ret;

  ret = GetNodes(topNode);
  if(ret != Fun4AllReturnCodes::EVENT_OK) return ret;

  ret = InitField(topNode);
  if(ret != Fun4AllReturnCodes::EVENT_OK) return ret;

  ret = InitGeom(topNode);
  if(ret != Fun4AllReturnCodes::EVENT_OK) return ret;

  PHField* field = PHFieldUtility::GetFieldMapNode(nullptr, topNode);
  assert(field);

  if(verbosity > 2) {
    cout << "PHField check: " << "-------" << endl;
    std::ofstream field_scan("field_scan.csv");
    field->identify(field_scan);
    field_scan.close();
  }

  _kmfit = KalmanFilter::instance();
  _kmfit->enableDumpCorrection();
  _extrapolator.init(field, _t_geo_manager);

  ///Single track finding doesn't require a propagation matrix
  _extrapolator.setPropCalc(false);
  _extrapolator.setLengthCalc(false);

  return Fun4AllReturnCodes::EVENT_OK;
}

int VertexFit::process_event(PHCompositeNode* topNode) {

  if(verbosity > 0) _recEvent->identify();

  _recEvent->clearDimuons(); // In case the vertexing is re-done.

  int ret = -99;
  try {
    ret = setRecEvent(_recEvent);
    _recEvent->setRecStatus(ret);
    if(Verbosity()>Fun4AllBase::VERBOSITY_A_LOT) {
      LogInfo("Vertexing Returned: " << ret);
    }
  } catch (...) {
    LogInfo("VertexFitting failed");
  }

  return Fun4AllReturnCodes::EVENT_OK;
}

int VertexFit::End(PHCompositeNode* topNode) {
  return Fun4AllReturnCodes::EVENT_OK;
}

int VertexFit::MakeNodes(PHCompositeNode* topNode) {
  return Fun4AllReturnCodes::EVENT_OK;
}

int VertexFit::GetNodes(PHCompositeNode* topNode) {

  _recEvent = findNode::getClass<SRecEvent>(topNode, "SRecEvent");
  if (!_recEvent) {
    if(Verbosity() > 2) LogInfo("!_recEvent");
    return Fun4AllReturnCodes::ABORTEVENT;
  }

  return Fun4AllReturnCodes::EVENT_OK;
}


int VertexFit::InitField(PHCompositeNode *topNode)
{
  if (verbosity > 1) cout << "VertexFit::InitField" << endl;
  PHField * phfield = PHFieldUtility::GetFieldMapNode(nullptr, topNode);

  if(phfield && verbosity > 1)
    cout << "VertexFit::InitField - use filed from NodeTree." << endl;

  if(!phfield) {
    if (verbosity > 1) cout << "VertexFit::InitField - create magnetic field setup" << endl;
    unique_ptr<PHFieldConfig> default_field_cfg(nullptr);
    default_field_cfg.reset(new PHFieldConfig_v3(recoConsts::instance()->get_CharFlag("fMagFile"), recoConsts::instance()->get_CharFlag("kMagFile"), recoConsts::instance()->get_DoubleFlag("FMAGSTR"), recoConsts::instance()->get_DoubleFlag("KMAGSTR"), 5.));
    phfield = PHFieldUtility::GetFieldMapNode(default_field_cfg.get(), topNode, 0);
  }

  assert(phfield);

  return Fun4AllReturnCodes::EVENT_OK;
}

int VertexFit::InitGeom(PHCompositeNode *topNode)
{
  if (verbosity > 1) cout << "VertexFit::InitGeom" << endl;

  _t_geo_manager = PHGeomUtility::GetTGeoManager(topNode);

  if(_t_geo_manager && verbosity > 1)
    cout << "VertexFit::InitGeom - use geom from NodeTree." << endl;

  assert(_t_geo_manager);

  return Fun4AllReturnCodes::EVENT_OK;
}

int VertexFit::setRecEvent(SRecEvent* recEvent, int sign1, int sign2)
{  
  std::vector<int> idx_pos = recEvent->getChargedTrackIDs(sign1);
  std::vector<int> idx_neg = recEvent->getChargedTrackIDs(sign2);

  nPos = idx_pos.size();
  nNeg = idx_neg.size();
  if(nPos*nNeg == 0) return VFEXIT_FAIL_DIMUONPAIR;

  //Prepare evaluation output
  if(evalTree != nullptr)
  {
      runID = recEvent->getRunID();
      eventID = recEvent->getEventID();
      targetPos = recEvent->getTargetPos();
  }

  //Loop over all possible combinations
  for(int i = 0; i < nPos; ++i)
    {
      if(!recEvent->getTrack(idx_pos[i]).isValid()) continue;
      if(Verbosity()>Fun4AllBase::VERBOSITY_A_LOT) {
        LogInfo("pos track OK");
      }

      int j = sign1 + sign2 == 0 ? 0 : i + 1;      // this is to avoid using same track twice in like-sign mode
      for(; j < nNeg; ++j)
      {
          //Only needed for like-sign muons
          if(idx_pos[i] == idx_neg[j]) continue;
          if(Verbosity()>Fun4AllBase::VERBOSITY_A_LOT) {
            LogInfo("two track OK");
          }

          SRecTrack track_neg = recEvent->getTrack(idx_neg[j]);
          if(!track_neg.isValid()) continue;
          if(Verbosity()>Fun4AllBase::VERBOSITY_A_LOT) {
            LogInfo("neg track OK");
          }
          SRecTrack track_pos = recEvent->getTrack(idx_pos[i]);

          SRecDimuon dimuon;
          dimuon.trackID_pos = idx_pos[i];
          dimuon.trackID_neg = idx_neg[j];

          dimuon.p_pos_single = track_pos.getMomentumVertex();
          dimuon.p_neg_single = track_neg.getMomentumVertex();
          dimuon.vtx_pos = track_pos.getVertex();
          dimuon.vtx_neg = track_neg.getVertex();
          dimuon.chisq_single = track_pos.getChisqVertex() + track_neg.getChisqVertex();

          //Start prepare the vertex fit
          init();
          if(KMAGSTR > 0)
          {
              addTrack(0, track_pos);
              addTrack(1, track_neg);
          }
          else
          {
              addTrack(0, track_neg);
              addTrack(1, track_pos);
          }
          // check if it decays between 500 and 600 cm first

	  TVector3 mom1 = track_pos.getMomentumVecSt1();
	  double tx_1 = mom1.Px() / mom1.Pz();
	  double ty_1 = mom1.Py() / mom1.Pz();
	  
	  TVector3 mom2 = track_neg.getMomentumVecSt1();
	  double tx_2 = mom2.Px() / mom2.Pz();
	  double ty_2 = mom2.Py() / mom2.Pz();

	  double zintX = (track_pos.getPositionVecSt1().X() - tx_1*track_pos.getPositionVecSt1().Z() - track_neg.getPositionVecSt1().X() + tx_2*track_neg.getPositionVecSt1().Z())/(tx_2 - tx_1);
	  double xintX = ( tx_1*zintX + track_pos.getPositionVecSt1().X() - tx_1*track_pos.getPositionVecSt1().Z() + tx_2*zintX + track_neg.getPositionVecSt1().X() - tx_2*track_pos.getPositionVecSt1().Z() )/2.;
	  double yintX = ( ty_1*zintX + track_pos.getPositionVecSt1().Y() - ty_1*track_pos.getPositionVecSt1().Z() + ty_2*zintX + track_neg.getPositionVecSt1().Y() - ty_2*track_pos.getPositionVecSt1().Z() )/2.;

	  double zintY = (track_pos.getPositionVecSt1().Y() - ty_1*track_pos.getPositionVecSt1().Z() - track_neg.getPositionVecSt1().Y() + ty_2*track_neg.getPositionVecSt1().Z())/(ty_2 - ty_1);
	  double xintY = ( tx_1*zintY + track_pos.getPositionVecSt1().X() - tx_1*track_pos.getPositionVecSt1().Z() + tx_2*zintY + track_neg.getPositionVecSt1().X() - tx_2*track_pos.getPositionVecSt1().Z() )/2.;
	  double yintY = ( ty_1*zintY + track_pos.getPositionVecSt1().Y() - ty_1*track_pos.getPositionVecSt1().Z() + ty_2*zintY + track_neg.getPositionVecSt1().Y() - ty_2*track_pos.getPositionVecSt1().Z() )/2.;

	  m_displaced = false;
	  if(!(std::abs(zintX - zintY)>500) && !(zintX > 800 && zintY > 800) && !(zintX > 850)){
          TVector3 posSt1 = findVertexDumpSt1(track_pos, track_neg);
	  if(posSt1.Z() < 594 && posSt1.Z() > 480){
	    m_displaced = true;
	    addHypothesis(posSt1.X(), posSt1.Y(), posSt1.Z(), 10.0, 10.0, 10.0);
	  }
	  else if(!(zintY < 0 && zintX > 0)){
	    m_displaced = true;
	    if( std::abs( zintX - zintY ) < 30 && (zintX > 500 || zintY > 500)){
	      addHypothesis(xintX, yintX, zintX, 10.0, 10.0, 10.0);
	      addHypothesis(xintY, yintY, zintY, 10.0, 10.0, 10.0);
	    } else if(zintX > 500 || zintY > 500){
	      for(double zv = 510; zv<610; zv=zv+10){
		TVector3 posSt1 = Interpolator(track_pos, track_neg, zv);
		addHypothesis(posSt1.X(), posSt1.Y(), posSt1.Z(), 10.0, 10.0, 10.0);
	      }  
	    }
	  }
	  if(posSt1.Z()<490){
	    addHypothesis(xintY, yintY, zintY, 10.0, 10.0, 10.0);
	    addHypothesis(xintX, yintX, zintX, 10.0, 10.0, 10.0);
	  }
	  }
	  
	  if(!m_displaced){
	    addHypothesis(0.5*(dimuon.vtx_pos[0] + dimuon.vtx_neg[0]), 0.5*(dimuon.vtx_pos[1] + dimuon.vtx_neg[1]), 0.5*(dimuon.vtx_pos[2] + dimuon.vtx_neg[2]), 10.0, 10.0, 50.);
	    TVector3 posCand = findDimuonVertexFast(track_pos, track_neg);
	    addHypothesis(posCand.X(), posCand.Y(), posCand.Z(),  10.0, 10.0, 50.);
	  }
          choice_eval = processOnePair();

          //Fill the dimuon info which are not related to track refitting first
          dimuon.chisq_vx = getVXChisq();
#ifdef _DEBUG_ON
          std::cout << "after processing one piar, vtx chisq " << getVXChisq() << " kalman chisq " << getKFChisq() << std::endl;
#endif
          dimuon.vtx.SetXYZ(_vtxpar_curr._r[0][0], _vtxpar_curr._r[1][0], _vtxpar_curr._r[2][0]);

          dimuon.proj_target_pos = track_pos.getTargetPos();
          dimuon.proj_dump_pos = track_pos.getDumpPos();
          dimuon.proj_target_neg = track_neg.getTargetPos();
          dimuon.proj_dump_neg = track_neg.getDumpPos();

          //Retrieve the results
          double z_vertex_opt = getVertexZ0();
          if(optimize)
          {   
              //if(z_vertex_opt < -80. && getKFChisq() < 10.) z_vertex_opt = Z_TARGET;
              if(dimuon.proj_target_pos.Perp() < dimuon.proj_dump_pos.Perp() && dimuon.proj_target_neg.Perp() < dimuon.proj_dump_neg.Perp())
              {
                  int nTry = 0;
                  double z_curr = 9999.;
                  while(fabs(z_curr - z_vertex_opt) > 0.5 && nTry < 100)
                  {
                      z_curr = z_vertex_opt;
                      ++nTry;

                      track_pos.setZVertex(z_vertex_opt);
                      track_neg.setZVertex(z_vertex_opt);

                      double m = (track_pos.getMomentumVertex() + track_neg.getMomentumVertex()).M();
                      //z_vertex_opt = -189.6 + 17.71*m - 1.159*m*m;    //parameterization of r1.4.0
                      //z_vertex_opt = -305.465 + 104.731*m - 24.3589*m*m + 2.5564*m*m*m - 0.0978876*m*m*m*m; //for E906 geomtery

 		      z_vertex_opt = -310.0540 + 4.3539*m - 0.9518*m*m + 0.0803*m*m*m ;//use this parametrization for E1039 geometry for now, more correction is due (Abi)

                      //std::cout << nTry << "  " << z_curr << "  " << z_vertex_opt << "  " << (track_pos.getMomentumVertex() + track_neg.getMomentumVertex()).M() << std::endl;
                  }
              }
          }

	  if(fit_target_center) z_vertex_opt = Z_TARGET; //fit in the target center

          track_pos.setZVertex(z_vertex_opt);
          track_neg.setZVertex(z_vertex_opt);
          dimuon.p_pos = track_pos.getMomentumVertex();
          dimuon.p_neg = track_neg.getMomentumVertex();
          dimuon.chisq_kf = track_pos.getChisqVertex() + track_neg.getChisqVertex();

          /*
          //If we are running in the like-sign mode, reverse one sign of px
          if(sign1 + sign2 != 0)
          {
              if(dimuon.p_pos.Px() < 0)
              {
                  dimuon.p_pos.SetPx(-dimuon.p_pos.Px());
                  dimuon.p_pos_single.SetPx(-dimuon.p_pos_single.Px());
              }
              if(dimuon.p_neg.Px() > 0)
              {
                  dimuon.p_neg.SetPx(-dimuon.p_neg.Px());
                  dimuon.p_neg_single.SetPx(-dimuon.p_neg_single.Px());
              }
              if(dimuon.p_pos.Py()*dimuon.p_neg.Py() > 0)
              {
                  dimuon.p_pos.SetPy(-dimuon.p_pos.Py());
                  dimuon.p_pos_single.SetPy(-dimuon.p_pos_single.Py());
              }
          }
          */
          dimuon.calcVariables();

          //Test three fixed hypothesis
          dimuon.chisq_dump = track_pos.getChisqDump() + track_neg.getChisqDump();
          dimuon.chisq_target = track_pos.getChisqTarget() + track_neg.getChisqTarget();
          dimuon.chisq_upstream = track_pos.getChisqUpstream() + track_neg.getChisqUpstream();

          //Fill the final data
          recEvent->insertDimuon(dimuon);

          //Fill the evaluation data
          p_idx_eval = dimuon.trackID_pos;
          m_idx_eval = dimuon.trackID_neg;
          fillEvaluation();
      }
  }

  if(recEvent->getNDimuons() > 0) return VFEXIT_SUCCESS;
  return VFEXIT_FAIL_ITERATION;
}

TVector3 VertexFit::Interpolator(SRecTrack& track1, SRecTrack& track2, double zval)
{
  TVector3 mom1 = track1.getMomentumVecSt1();
  double tx_1 = mom1.Px() / mom1.Pz();
  double ty_1 = mom1.Py() / mom1.Pz();

  TVector3 mom2 = track2.getMomentumVecSt1();
  double tx_2 = mom2.Px() / mom2.Pz();
  double ty_2 = mom2.Py() / mom2.Pz();

  double xint = ( tx_1*zval + track1.getPositionVecSt1().X() - tx_1*track1.getPositionVecSt1().Z() + tx_2*zval + track2.getPositionVecSt1().X() - tx_2*track1.getPositionVecSt1().Z() )/2.;
  double yint = ( ty_1*zval + track1.getPositionVecSt1().Y() - ty_1*track1.getPositionVecSt1().Z() + ty_2*zval + track2.getPositionVecSt1().Y() - ty_2*track1.getPositionVecSt1().Z() )/2.;

  return TVector3( xint, yint, zval );
}

TVector3 VertexFit::findVertexDumpSt1(SRecTrack& track1, SRecTrack& track2)
{
    int NSteps_St1 = 110;
    double step_st1 = 110.0/NSteps_St1;

    //Try to find a vertex candidate between the downstream face of FMag and Track station1
    TVector3 pos1[NSteps_St1 + 1];
    TVector3 pos2[NSteps_St1 + 1];
    pos1[0] = track1.getPositionVecSt1();
    pos2[0] = track2.getPositionVecSt1();

    // assume FMag has no effect between downstream face and the station1
    TVector3 mom1 = track1.getMomentumVecSt1();
    double tx_1 = mom1.Px() / mom1.Pz();
    double ty_1 = mom1.Py() / mom1.Pz();

    TVector3 mom2 = track2.getMomentumVecSt1();
    double tx_2 = mom2.Px() / mom2.Pz();
    double ty_2 = mom2.Py() / mom2.Pz();
    
    // make the swim from Station1 to FMag
    for (int iStep = 1; iStep < NSteps_St1; ++iStep) {
       TVector3 trajVec1(tx_1*step_st1, ty_1*step_st1, step_st1); 
       TVector3 trajVec2(tx_2*step_st1, ty_2*step_st1, step_st1);

       pos1[iStep] = pos1[iStep - 1] - trajVec1;
       pos2[iStep] = pos2[iStep - 1] - trajVec2;
    }

    // find the closest distance
    int iStep_min = -1;
    double dist_min = 10000;
    for (int iStep = 0; iStep < NSteps_St1; ++iStep) {
        double dist = (pos1[iStep] - pos2[iStep]).Perp();
        if(dist < dist_min) {
            iStep_min = iStep;
            dist_min = dist;
        }
    }

    //if(iStep_min == -1 || dist_min > 5.0) {
    if(iStep_min == -1) {
        return TVector3(0., 0., -9999.);
    }
    TVector3 pos_sum = pos1[iStep_min] + pos2[iStep_min];
    return TVector3( pos_sum.X()/2.0, pos_sum.Y()/2.0, pos_sum.Z()/2.0 );
}

TVector3 VertexFit::findDimuonVertexFast(SRecTrack& track1, SRecTrack& track2)
{
    //Swim both tracks all the way down, and store the numbers
    TVector3 pos1[NSTEPS_FMAG + NSTEPS_TARGET + 1];
    TVector3 pos2[NSTEPS_FMAG + NSTEPS_TARGET + 1];
    TVector3 mom1[NSTEPS_FMAG + NSTEPS_TARGET + 1];
    TVector3 mom2[NSTEPS_FMAG + NSTEPS_TARGET + 1];
    track1.swimToVertex(pos1, mom1);
    track2.swimToVertex(pos2, mom2);

    int iStep_min = -1;
    double dist_min = 1E6;
    int charge1 = track1.getCharge();
    int charge2 = track2.getCharge();
    for(int iStep = 0; iStep < NSTEPS_FMAG + NSTEPS_TARGET + 1; ++iStep)
    {
        double dist = (pos1[iStep] - pos2[iStep]).Perp();
        if(dist < dist_min && FMAGSTR*charge1*mom1[iStep].Px() > 0 && FMAGSTR*charge2*mom2[iStep].Px() > 0)
        {
            iStep_min = iStep;
            dist_min = dist;
        }
    }

    if(iStep_min == -1) return TVector3(0., 0., Z_DUMP);
    TVector3 pos_sum = pos1[iStep_min] + pos2[iStep_min];
    return TVector3( pos_sum.X()/2.0, pos_sum.Y()/2.0, pos_sum.Z()/2.0 );
    //return pos1[iStep_min].Z();
}

void VertexFit::init()
{
    _trkpar_curr.clear();

    z_vertex.clear();
    x_vertex.clear();
    y_vertex.clear();
    r_vertex.clear();
    chisq_km.clear();
    chisq_vx.clear();

    z_start.clear();
    sig_z_start.clear();
    x_start.clear();
    sig_x_start.clear();
    y_start.clear();
    sig_y_start.clear();

    ///Two default starting points
    //addHypothesis(Z_DUMP, 50.);
    //addHypothesis(Z_TARGET, 50.);
}

void VertexFit::addHypothesis(double x, double y, double z, double sigx, double sigy, double sigz) 
{
    x_start.push_back(x);
    sig_x_start.push_back(sigx);

    y_start.push_back(y);
    sig_y_start.push_back(sigy);

    z_start.push_back(z);
    sig_z_start.push_back(sigz);
}

void VertexFit::setStartingVertex(double x_start, double sigx_start, double y_start, double sigy_start, double z_start, double sigz_start)
{
    ///Initialize the starting vertex with a guess and large error
    _vtxpar_curr._r[0][0] = x_start;
    _vtxpar_curr._r[1][0] = y_start;
    _vtxpar_curr._r[2][0] = z_start;

    _vtxpar_curr._cov.Zero();
    _vtxpar_curr._cov[0][0] = sigx_start*sigx_start;
    _vtxpar_curr._cov[1][1] = sigy_start*sigy_start;
    _vtxpar_curr._cov[2][2] = sigz_start*sigz_start;

    _chisq_vertex = 0.;
    _chisq_kalman = 0.;
}

int VertexFit::processOnePair()
{
    double chisq_min = 1E6;
    int index_min = -1;

    nStart = z_start.size();
    for(int i = 0; i < nStart; ++i)
    {
#ifdef _DEBUG_ON
      LogInfo("Testing starting point: " << z_start[i]<<" (with "<<x_start[i]<<" "<<y_start[i]<<")");
#endif

        setStartingVertex(x_start[i], sig_x_start[i], y_start[i], sig_y_start[i], z_start[i], sig_z_start[i]);
        nIter_eval[i] = findVertex();

        chisq_km.push_back(_chisq_kalman);
        chisq_vx.push_back(_chisq_vertex);
        z_vertex.push_back(_vtxpar_curr._r[2][0]);
        x_vertex.push_back(_vtxpar_curr._r[0][0]);
        y_vertex.push_back(_vtxpar_curr._r[1][0]);
        r_vertex.push_back(sqrt(_vtxpar_curr._r[0][0]*_vtxpar_curr._r[0][0] + _vtxpar_curr._r[1][0]*_vtxpar_curr._r[1][0]));
#ifdef _DEBUG_ON
        std::cout << "after findVertex, position: " << _vtxpar_curr._r[0][0] << " " << _vtxpar_curr._r[1][0] << " " << _vtxpar_curr._r[2][0]  << " chi2 kalman " << _chisq_kalman << " chi2 vtx " << _chisq_vertex <<std::endl;
#endif

	if(m_displaced){
	  if(_chisq_vertex < chisq_min && _chisq_kalman < 99999 && _chisq_vertex > 0.00001)
	    {
	      index_min = i;
	      chisq_min = _chisq_vertex;
	    }
	} else{
	  if(_chisq_kalman < chisq_min)
	    {
	      index_min = i;
	      chisq_min = _chisq_kalman;
	    }
	}
    }

    if(index_min < 0) return 0;

    _chisq_kalman = chisq_km[index_min];
    _chisq_vertex = chisq_vx[index_min];
    _vtxpar_curr._r[0][0] = x_vertex[index_min];
    _vtxpar_curr._r[1][0] = y_vertex[index_min];
    _vtxpar_curr._r[2][0] = z_vertex[index_min];
    if(z_vertex[index_min] > 1E4)
    {
        index_min = z_start.size();
        _vtxpar_curr._r[2][0] = z_start.back();
    }

#ifdef _DEBUG_ON
    std::cout << "at the end of processing one pair, position " << _vtxpar_curr._r[0][0] << " " << _vtxpar_curr._r[1][0] << " " << _vtxpar_curr._r[2][0]  << " chi2 kalman " << _chisq_kalman << " chi2 vtx " << _chisq_vertex <<std::endl;
#endif

    return index_min;
}

void VertexFit::addTrack(int index, KalmanTrack& _track)
{
    if(_track.getNodeList().front().isSmoothDone())
    {
        _trkpar_curr.push_back(_track.getNodeList().front().getSmoothed());
    }
    else if(_track.getNodeList().front().isFilterDone())
    {
        _trkpar_curr.push_back(_track.getNodeList().front().getFiltered());
    }
    else
    {
        _trkpar_curr.push_back(_track.getNodeList().front().getPredicted());
    }
}

void VertexFit::addTrack(int index, SRecTrack& _track)
{
    TrkPar _trkpar;
    _trkpar._state_kf = _track.getStateVector(0);
    _trkpar._covar_kf = _track.getCovariance(0);
    _trkpar._z = _track.getZ(0);

    _trkpar_curr.push_back(_trkpar);
}

void VertexFit::addTrack(int index, TrkPar& _trkpar)
{
    _trkpar_curr.push_back(_trkpar);
}

int VertexFit::findVertex()
{

  double x_start = _vtxpar_curr._r[0][0];
  double y_start = _vtxpar_curr._r[1][0];
  double z_start = _vtxpar_curr._r[2][0];

  double sigx_start_2 = _vtxpar_curr._cov[0][0];
  double sigy_start_2 = _vtxpar_curr._cov[1][1];
  double sigz_start_2 = _vtxpar_curr._cov[2][2];
  
    int nIter = 0;
    double _chisq_kalman_prev = 1E6;
    for(; nIter < _max_iteration; ++nIter)
    {
#ifdef _DEBUG_ON
      LogInfo("Iteration: " << nIter);
      std::cout << "iteration " << nIter << " vtx position " << _vtxpar_curr._r[0][0] << " " << _vtxpar_curr._r[1][0] << " " << _vtxpar_curr._r[2][0]  << " chi2 kalman " << _chisq_kalman << " chi2 vtx " << _chisq_vertex <<std::endl;
#endif
	  _chisq_vertex = 0.;
	  _chisq_kalman = 0.;
#ifdef _DEBUG_ON
	LogInfo("_trkpar_curr.size() = "<<_trkpar_curr.size());
#endif
        for(unsigned int j = 0; j < _trkpar_curr.size(); j++)
        {
            _node_vertex.resetFlags();
            //_node_vertex.getMeasurement() = _vtxpar_curr._r.GetSub(0, 1, 0, 0);
            //_node_vertex.getMeasurementCov() = _vtxpar_curr._cov.GetSub(0, 1, 0, 1);
#ifdef _DEBUG_ON
	    LogInfo("_vtxpar_curr._r[2][0] = "<<_vtxpar_curr._r[2][0]<<" and _trkpar_curr[j]._z = "<<_trkpar_curr[j]._z);
#endif
	    _node_vertex.setZ(_vtxpar_curr._r[2][0]);

            _kmfit->setCurrTrkpar(_trkpar_curr[j]);
            if(!_kmfit->fit_node(_node_vertex))
            {
#ifdef _DEBUG_ON
                LogInfo("Vertex fit for this track failed!");
#endif
                _chisq_kalman = 1E5;
                break;
            }
            else
            {
                _chisq_kalman += _node_vertex.getChisq();
            }

            updateVertex();
	}

        ///break the iteration if the z0 converges
#ifdef _DEBUG_ON
        LogInfo("At this iteration: " << nIter);
        LogInfo(_vtxpar_curr._r[2][0] << " ===  " << _node_vertex.getZ() << " : " << _chisq_kalman << " === " << _chisq_vertex << " : " << _vtxpar_curr._r[0][0] << " === " << _vtxpar_curr._r[1][0] << " _chisq_kalman_prev = "<< _chisq_kalman_prev);
#endif
        //if(_vtxpar_curr._r[2][0] < Z_UPSTREAM || _vtxpar_curr._r[2][0] > Z_DOWNSTREAM)
        if(_vtxpar_curr._r[2][0] < Z_UPSTREAM)
        {
            _chisq_kalman = 1E5;
            _chisq_vertex = 1E5;
            break;
        }

	if(m_displaced){
	  if(nIter > 0 && (fabs(_vtxpar_curr._r[2][0] - _node_vertex.getZ()) < _tolerance || _chisq_vertex > _chisq_kalman_prev)) break;
	  _chisq_kalman_prev = _chisq_vertex;
	} else{
	  if(nIter > 0 && (fabs(_vtxpar_curr._r[2][0] - _node_vertex.getZ()) < _tolerance || _chisq_kalman > _chisq_kalman_prev)) break;
	  _chisq_kalman_prev = _chisq_kalman;
	}
    }

    if(!m_displaced) return nIter+1;

    _vtxpar_curr._r[0][0] = x_start;
    _vtxpar_curr._r[1][0] = y_start;
    _vtxpar_curr._r[2][0] = z_start;

    _vtxpar_curr._cov.Zero();
    _vtxpar_curr._cov[0][0] = sigx_start_2;
    _vtxpar_curr._cov[1][1] = sigy_start_2;
    _vtxpar_curr._cov[2][2] = sigz_start_2;

    _chisq_vertex = 0.;
    _chisq_kalman = 0.;
    
    for(unsigned int it = 0; it < nIter; ++it)
    {
#ifdef _DEBUG_ON
      LogInfo("REDO Iteration: " << it);
      std::cout << "iteration " << it << " vtx position " << _vtxpar_curr._r[0][0] << " " << _vtxpar_curr._r[1][0] << " " << _vtxpar_curr._r[2][0]  << " chi2 kalman " << _chisq_kalman << " chi2 vtx " << _chisq_vertex <<std::endl;
#endif
	  _chisq_vertex = 0.;
	  _chisq_kalman = 0.;
#ifdef _DEBUG_ON
	LogInfo("_trkpar_curr.size() = "<<_trkpar_curr.size());
#endif
        for(unsigned int j = 0; j < _trkpar_curr.size(); j++)
        {
            _node_vertex.resetFlags();
#ifdef _DEBUG_ON
	    LogInfo("_vtxpar_curr._r[2][0] = "<<_vtxpar_curr._r[2][0]<<" and _trkpar_curr[j]._z = "<<_trkpar_curr[j]._z);
#endif
	    _node_vertex.setZ(_vtxpar_curr._r[2][0]);

            _kmfit->setCurrTrkpar(_trkpar_curr[j]);
            if(!_kmfit->fit_node(_node_vertex))
            {
#ifdef _DEBUG_ON
                LogInfo("Vertex fit for this track failed!");
#endif
                _chisq_kalman = 1E5;
                break;
            }
            else
            {
                _chisq_kalman += _node_vertex.getChisq();
            }

            updateVertex();
	}

        ///break the iteration if the z0 converges
#ifdef _DEBUG_ON
        LogInfo("At this iteration: " << it);
        LogInfo(_vtxpar_curr._r[2][0] << " ===  " << _node_vertex.getZ() << " : " << _chisq_kalman << " === " << _chisq_vertex << " : " << _vtxpar_curr._r[0][0] << " === " << _vtxpar_curr._r[1][0] << " _chisq_kalman_prev = "<< _chisq_kalman_prev);
#endif
        //if(_vtxpar_curr._r[2][0] < Z_UPSTREAM || _vtxpar_curr._r[2][0] > Z_DOWNSTREAM)
        if(_vtxpar_curr._r[2][0] < Z_UPSTREAM)
        {
            _chisq_kalman = 1E5;
            _chisq_vertex = 1E5;
            break;
        }

	if(m_displaced){
	  if(it > 0 && (fabs(_vtxpar_curr._r[2][0] - _node_vertex.getZ()) < _tolerance || _chisq_vertex > _chisq_kalman_prev)) break;
	  _chisq_kalman_prev = _chisq_vertex;
	} else{
	  if(it > 0 && (fabs(_vtxpar_curr._r[2][0] - _node_vertex.getZ()) < _tolerance || _chisq_kalman > _chisq_kalman_prev)) break;
	  _chisq_kalman_prev = _chisq_kalman;
	}
    }

    
    return nIter;
}

void VertexFit::updateVertex()
{
    double p = fabs(1./_node_vertex.getFiltered()._state_kf[0][0]);
    double px = _node_vertex.getFiltered()._state_kf[1][0];
    double py = _node_vertex.getFiltered()._state_kf[2][0];
    double pz = sqrt(p*p - px*px - py*py);
    
    ///Set the projector matrix from track state vector to the coordinate
    TMatrixD H(2, 3);
    H.Zero();

    H[0][0] = 1.;
    H[1][1] = 1.;
    H[0][2] = -px/pz;
    H[1][2] = -py/pz;
    
    TMatrixD vertex_dummy(3, 1);
    vertex_dummy.Zero();
    vertex_dummy[2][0] = _vtxpar_curr._r[2][0];
    
    TMatrixD mxy = _node_vertex.getFiltered()._state_kf.GetSub(3, 4, 0, 0);
    TMatrixD Vxy = _node_vertex.getFiltered()._covar_kf.GetSub(3, 4, 3, 4);
    TMatrixD S = SMatrix::invertMatrix(Vxy + SMatrix::getABCt(H, _vtxpar_curr._cov, H));
    TMatrixD K = SMatrix::getABtC(_vtxpar_curr._cov, H, S);
    TMatrixD zeta = mxy - H*(_vtxpar_curr._r - vertex_dummy);
    TMatrixD _r_filtered = _vtxpar_curr._r + K*zeta;
    TMatrixD _cov_filtered = _vtxpar_curr._cov - K*H*(_vtxpar_curr._cov);
    _chisq_vertex += SMatrix::getAtBC(zeta, S, zeta)[0][0];
    _vtxpar_curr._r = _r_filtered;
    _vtxpar_curr._cov = _cov_filtered;
}

double VertexFit::findSingleMuonVertex(SRecTrack& _track)
{
    TrkPar _trkpar_start;
    _trkpar_start._state_kf = _track.getStateVector(0);
    _trkpar_start._covar_kf = _track.getCovariance(0);
    _trkpar_start._z = _track.getZ(0);

    return findSingleMuonVertex(_trkpar_start);
}

double VertexFit::findSingleMuonVertex(Node& _node_start)
{
    TrkPar _trkpar_start;
    if(_node_start.isSmoothDone())
    {
        _trkpar_start = _node_start.getSmoothed();
    }
    else if(_node_start.isFilterDone())
    {
        _trkpar_start = _node_start.getFiltered();
    }
    else
    {
        _trkpar_start = _node_start.getPredicted();
    }

    return findSingleMuonVertex(_trkpar_start);
}

double VertexFit::findSingleMuonVertex(TrkPar& _trkpar_start)
{
    _extrapolator.setInitialStateWithCov(_trkpar_start._z, _trkpar_start._state_kf, _trkpar_start._covar_kf);
    double z_vertex_single = _extrapolator.extrapolateToIP();

    return z_vertex_single;
}

void VertexFit::bookEvaluation(std::string evalFileName)
{
    evalFile = new TFile(evalFileName.c_str(), "recreate");
    evalTree = new TTree("T", "VertexFit eval");

    evalTree->Branch("runID", &runID, "runID/I");
    evalTree->Branch("eventID", &eventID, "eventID/I");
    evalTree->Branch("targetPos", &targetPos, "targetPos/I");
    evalTree->Branch("choice_eval", &choice_eval, "choice_eval/I");
    evalTree->Branch("choice_by_kf_eval", &choice_by_kf_eval, "choice_by_kf_eval/I");
    evalTree->Branch("choice_by_vx_eval", &choice_by_vx_eval, "choice_by_vx_eval/I");

    evalTree->Branch("nStart", &nStart, "nStart/I");
    evalTree->Branch("nIter_eval", nIter_eval, "nIter_eval[nStart]/I");
    evalTree->Branch("chisq_kf_eval", chisq_kf_eval, "chisq_kf_eval[nStart]/D");
    evalTree->Branch("chisq_vx_eval", chisq_vx_eval, "chisq_vx_eval[nStart]/D");
    evalTree->Branch("z_vertex_eval", z_vertex_eval, "z_vertex_eval[nStart]/D");
    evalTree->Branch("r_vertex_eval", r_vertex_eval, "r_vertex_eval[nStart]/D");
    evalTree->Branch("z_start_eval", z_start_eval, "z_start_eval[nStart]/D");

    evalTree->Branch("m_chisq_kf_eval", &m_chisq_kf_eval, "m_chisq_kf_eval/D");
    evalTree->Branch("m_z_vertex_eval", &m_z_vertex_eval, "m_z_vertex_eval/D");
    evalTree->Branch("s_chisq_kf_eval", &s_chisq_kf_eval, "s_chisq_kf_eval/D");
    evalTree->Branch("s_z_vertex_eval", &s_z_vertex_eval, "s_z_vertex_eval/D");
}

void VertexFit::fillEvaluation()
{
    if(evalTree == nullptr) return;

    choice_by_kf_eval = -1;
    choice_by_vx_eval = -1;
    double chisq_kf_min = 1E6;
    double chisq_vx_min = 1E6;

    m_chisq_kf_eval = 0.;
    m_z_vertex_eval = 0.;
    for(int i = 0; i < nStart; ++i)
    {
        chisq_kf_eval[i] = chisq_km[i];
        chisq_vx_eval[i] = chisq_vx[i];
        z_vertex_eval[i] = z_vertex[i];
        r_vertex_eval[i] = r_vertex[i];
        z_start_eval[i] = z_start[i];

        m_chisq_kf_eval += chisq_kf_eval[i];
        m_z_vertex_eval += z_vertex_eval[i];

        if(chisq_kf_min > chisq_km[i])
        {
            choice_by_kf_eval = i;
            chisq_kf_min = chisq_km[i];
        }

        if(chisq_vx_min > chisq_vx[i])
        {
            choice_by_vx_eval = i;
            chisq_vx_min = chisq_vx[i];
        }
    }
    m_chisq_kf_eval /= nStart;
    m_z_vertex_eval /= nStart;

    s_chisq_kf_eval = 0.;
    s_z_vertex_eval = 0.;
    if(nStart == 1)
    {
        evalTree->Fill();
        return;
    }

    for(int i = 0; i < nStart; ++i)
    {
        s_chisq_kf_eval += ((m_chisq_kf_eval - chisq_kf_eval[i])*(m_chisq_kf_eval - chisq_kf_eval[i]));
        s_z_vertex_eval += ((m_z_vertex_eval - z_vertex_eval[i])*(m_z_vertex_eval - z_vertex_eval[i]));
    }
    s_chisq_kf_eval = sqrt(s_chisq_kf_eval/(nStart-1));
    s_z_vertex_eval = sqrt(s_z_vertex_eval/(nStart-1));

    evalTree->Fill();
}

void VertexFit::print()
{
    using namespace std;

    for(unsigned int i = 0; i < z_start.size(); i++)
    {
        cout << "============= Hypothesis " << i << " ============" << endl;
        cout << "z_start = " << z_start[i] << ", sigz_start = " << sig_z_start[i] << endl;
        cout << "Found z_vertex = " << z_vertex[i] << endl;
        cout << "With chisq_km = " << chisq_km[i] << " and chisq_vx = " << chisq_vx[i] << endl;
    }
}
