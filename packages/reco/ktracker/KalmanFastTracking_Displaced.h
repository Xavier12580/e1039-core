/*
KalmanFastTracking_Displaced.h

Fast tracking utility of Kalman filter track, used to improve the tracking speed and also for online monitoring

Author: Kun Liu, liuk@fnal.gov
Created: 05-24-2013
*/

#ifndef _KALMANFASTTRACKING_Displaced_H
#define _KALMANFASTTRACKING_Displaced_H

#include <GlobalConsts.h>
#include <geom_svc/GeomSvc.h>

#include <list>
#include <vector>
#include <map>

#include <Math/Factory.h>
#include <Math/Minimizer.h>
#include <Math/Functor.h>

#include "TMathBase.h"

#include "SRawEvent.h"
#include "KalmanTrack.h"
#include "KalmanFitter.h"
#include "FastTracklet.h"

class TGeoManager;

class PHField;
class PHTimer;

class KalmanFastTracking_Displaced
{
public:
    explicit KalmanFastTracking_Displaced(const PHField *field, const TGeoManager *geom, bool flag = true);
    ~KalmanFastTracking_Displaced();

    // set/get verbosity
    void Verbosity(const int a) { verbosity = a; }
    int Verbosity() const { return verbosity; }
    void printTimers();

    // Set the input event
    int setRawEvent(SRawEvent *event_input);
    void setRawEventDebug(SRawEvent *event_input);

    // Event quality cut
    bool acceptEvent(SRawEvent *rawEvent);

    void cutAdjuster(int numCombos, int pass);

    /// Tracklet finding stuff
    // Build tracklets in a station
    void buildTrackletsInStation(int stationID, int listID, double *pos_exp = nullptr, double *window = nullptr);
    void buildTrackletsInStationSlim(int stationID, int listID, int plane_type, double *pos_exp = nullptr, double *window = nullptr);

    bool buildTrackletsInStation1(int stationID, int listID, double expXZSlope, double expYSlope, double y0, bool tight, double *pos_exp = nullptr, double *window = nullptr);

    // Build back partial tracks using tracklets in station 2 & 3
    void buildBackPartialTracks();
    void buildFullBackPartialTracksSlim();
    int buildBackPartialTracksSlim();
    void buildBackPartialTracksSlim(int plane_type, int pass, double slopeComparison, double windowSize);

    void buildXUCombos();
    void buildXUVCombos();
    void buildCombos();
    // Build global tracks by connecting station 23 tracklets and station 1 tracklets
    void buildGlobalTracks();

    // Build global tracks by connecting station 23 tracklets and station 1 tracklets
    void buildGlobalTracksDisplaced();

    // Fit tracklets
    int fitTracklet(Tracklet &tracklet);

    // Check the quality of tracklet, number of hits
    bool acceptTracklet(Tracklet &tracklet);
    bool hodoMask(Tracklet &tracklet);
    bool muonID_comp(Tracklet &tracklet);
    bool muonID_search(Tracklet &tracklet);
    bool muonID_hodoAid(Tracklet &tracklet);

    bool compareTracklets(Tracklet &tracklet1, Tracklet &tracklet2);

    bool compareTrackletsSlim(Tracklet *tracklet1, Tracklet *tracklet2, int plane_type, int pass, double slopeComparison, double windowSize);
    bool compareTrackletsSlim_3hits(Tracklet *tracklet1, Tracklet *tracklet2, int plane_type, int pass, double slopeComparison, double windowSize);

    void buildPropSegments();

    // Resolve left-right when possible
    void resolveLeftRight(SRawEvent::hit_pair hpair, int &LR1, int &LR2);
    void resolveLeftRight(Tracklet &tracklet, double threshold);
    void resolveSingleLeftRight(Tracklet &tracklet);

    // void resolveStation1Hits(Tracklet tracklet1, Tracklet tracklet2);
    bool resolveStation1Hits();
    void checkIntercepts(Tracklet tracklet1, Tracklet tracklet2, double &xint, double &yint);

    // Remove bad hit if needed
    bool removeBadHits(Tracklet &tracklet);
    void checkSigns(Tracklet &tracklet);

    // Reduce the list of tracklets, returns the number of elements reduced
    int reduceTrackletList(std::list<Tracklet> &tracklets);

    // Get exp postion and window using sagitta method in station 1
    void getSagittaWindowsInSt1(Tracklet &tracklet, double *pos_exp, double *window, int st1ID);
    void getExtrapoWindowsInSt1(Tracklet &tracklet, double *pos_exp, double *window, int st1ID);

    // Print the distribution of tracklets at detector back/front
    void printAtDetectorBack(int stationID, std::string outputFileName);

    /// Track fitting stuff
    // Convert Tracklet to KalmanTrack and solve left-right problem, and eventually to a SRecTrack
    SRecTrack processOneTracklet(Tracklet &tracklet);

    // Use Kalman fitter to fit a track
    bool fitTrack(KalmanTrack &kmtrk);

    // Resolve left right by Kalman fitting results
    void resolveLeftRight(KalmanTrack &kmtrk);

    /// Final output
    std::list<Tracklet> &getFinalTracklets() { return trackletsInSt[outputListIdx]; }
    std::list<Tracklet> &getBackPartials() { return trackletsInSt[3]; }
    std::list<Tracklet> &getTrackletList(int i) { return trackletsInSt[i]; }
    std::list<SRecTrack> &getSRecTracks() { return stracks; }
    std::list<PropSegment> &getPropSegments(int i) { return propSegs[i]; }

    double getTotalTime() { return totalTime; }

    /// Set the index of the final output tracklet list
    // void setOutputListID(unsigned int i) { outputListIdx = i; }
    void setOutputListIndex(unsigned int i) { outputListIdx = i; }

    /// Tool, a simple-minded chi square fit
    void chi2fit(int n, double x[], double y[], double &a, double &b);
    void checkQuality(Tracklet &tracklet);
    void assignSign(SignedHit &hit, int line_index);
    // private:
protected:
    // verbosity following Fun4All convention
    int verbosity;

    // Raw event input
    SRawEvent *rawEvent;
    std::vector<Hit> hitAll;

    // Tracklets in one event, id = 0, 1, 2 for station 0/1, 2, 3+/-, id = 3 for station 2&3 combined, id = 4 for global tracks
    // Likewise for the next part

    std::list<Tracklet> trackletsInSt[5];
    std::list<Tracklet> trackletsInStSlim[5][3]; // 3 for 3 planetypes: X=0, U=1, V=2

    std::vector<Tracklet> trackletsInSt23Slim[3];

    std::list<Tracklet> globalTracklets;

    std::vector<std::vector<Tracklet>> globalTracklets_resolveSt1;

    int num1XCombos;
    int num1UCombos;
    int num1VCombos;

    // Final SRecTrack list
    std::list<SRecTrack> stracks;

    // Index of the trackletlist designated as output
    unsigned int outputListIdx;

    // Prop. tube segments for muon id purposes
    //  0 for X-Z, 1 for Y-Z
    std::list<PropSegment> propSegs[2];

    /// Configurations of tracklet finding
    // Hodo. IDs for masking, 4 means we have 4 hodo stations
    std::vector<int> detectorIDs_mask[4];
    std::vector<int> detectorIDs_maskX[4];
    std::vector<int> detectorIDs_maskY[4];
    std::list<int> hitIDs_mask[4];               // hits in T/B, L/R are combined
    std::list<int> hitIDs_maskX[4];              // hits in T/B, L/R are combined
    std::list<int> hitIDs_maskY[4];              // hits in T/B, L/R are combined
    std::vector<int> detectorIDs_muidHodoAid[2]; // Aux-hodoscope masking for muon ID

    // register difference hodo masking stations for different chamber detectors
    std::vector<int> stationIDs_mask[nStations];

    // prop. tube IDs for MUID -- 0 for x-z, 1 for y-z
    int detectorIDs_muid[2][4];
    double z_ref_muid[2][4];
    std::list<int> hitIDs_muid[2][4];
    std::list<int> hitIDs_muidHodoAid[2];

    // Masking window sizes, index is the uniqueID defined by nElement*detectorID + elementID
    double z_mask[nHodoPlanes + nPropPlanes];
    double x_mask_min[nHodoPlanes + nPropPlanes][72];
    double x_mask_max[nHodoPlanes + nPropPlanes][72];
    double y_mask_min[nHodoPlanes + nPropPlanes][72];
    double y_mask_max[nHodoPlanes + nPropPlanes][72];

    /// For following part, id = 0, 1, 2, 3, 4, 5, 6 stand for station 0, 1, 2, 3+, 3-, and prop tubes X-Z and Y-Z
    // Super plane IDs for DCs
    std::vector<int> superIDs[nChamberPlanes / 6 + 2];

    // Window sizes for X-U combination
    double u_win[nChamberPlanes / 6];
    double u_costheta[nChamberPlanes / 6];
    double u_sintheta[nChamberPlanes / 6];
    double z_plane_x[nChamberPlanes / 6];
    double z_plane_u[nChamberPlanes / 6];
    double z_plane_v[nChamberPlanes / 6];

    // Plane angles for all planes
    double costheta_plane[nChamberPlanes + 1];
    double sintheta_plane[nChamberPlanes + 1];

    // Z positions
    double z_plane[nChamberPlanes + 1];

    // Maximum slope and intersection in each view
    double slope_max[nChamberPlanes + 1];
    double intersection_max[nChamberPlanes + 1];

    // Resolutions of all planes
    double resol_plane[nChamberPlanes + 1];

    // Cell width of all planes
    double spacing_plane[nChamberPlanes + 1];

    // Sagitta ratio in station 1, index 0, 1, 2 are for X/U/V
    int s_detectorID[3];

    // Current tracklets being processed
    Tracklet tracklet_curr;

    // Least chi square fitter and functor
    ROOT::Math::Minimizer *minimizer[2];
    ROOT::Math::Functor fcn;

    // Kalman fitter
    KalmanFitter *kmfitter;

    // Geometry service
    GeomSvc *p_geomSvc;

    // Flag for enable Kalman fitting
    const bool enable_KF;

    // Timer
    std::map<std::string, PHTimer *> _timers;

    double m_slopeComparisonMedium = 0.07;
    double m_windowSizeMedium = 11.;

    double m_slopeComparisonTight = 0.04;
    double m_windowSizeTight = 7.;

    double m_slopeComparison = 0.5;
    double m_windowSize = 55.;

    double m_slopeComparisonSt1 = 0.45; // was 0.40, but there was an event lost because of a big st1 distortion; then was .45
    double m_XWindowSt1 = 1.5;
    double m_UVWindowSt1 = 3.5; // was 3.5

    double m_hodoXWindow = 20;
    double m_hodoUVWindow = 55;

    double m_hodoXDIFFWindow = 25;
    double m_hodoUVDIFFWindow = 30;

    double m_XUVSlopeWindowCoarse = 0.06; // was 0.04
    double m_XUVPosWindowCoarse = 12.;    // was 9.

    double m_YSlopesDiff = 0.01; // was 0.007
    double m_XSlopesDiff = 0.01; // was 0.007

    double m_chiSqCut = 300.;
    double m_st23ChiSqCut = 50.; // was 15, 20, then 40
    // double m_st23ChiSqCut = 20.; //was 15, 20, then 40

    double slopeComp;
    double windowSize;
    int reqHits;
    double slopeCompSt1;
    double XWinSt1;
    double UVWinSt1;
    double hodoXWin;
    double hodoUVWin;
    double hodoXDIFFWin;
    double hodoUVDIFFWin;
    double XUVSlopeWin;
    double XUVPosWin;
    double YSlopesDiff;
    double XSlopesDiff;
    double chiSqCut;
    double st23ChiSqCut;

    bool highPU;
    bool adjusted;

    double totalTime;

    int setRawEventPrep(SRawEvent *event_input);
};

#endif
