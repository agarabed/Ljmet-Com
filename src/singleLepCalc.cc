/*
 Calculator for a generic single lepton analysis
 
 Author: Joshua Swanson, 2014
 */

#include <iostream>
#include <limits>   // std::numeric_limits

#include "LJMet/Com/interface/BaseCalc.h"
#include "LJMet/Com/interface/LjmetFactory.h"
#include "LJMet/Com/interface/LjmetEventContent.h"
#include "TLorentzVector.h"

#include "DataFormats/HLTReco/interface/TriggerEvent.h"
#include "DataFormats/Common/interface/TriggerResults.h"
#include "DataFormats/PatCandidates/interface/TriggerObject.h"
#include "FWCore/Common/interface/TriggerNames.h"
#include "DataFormats/Math/interface/deltaR.h"
#include "EgammaAnalysis/ElectronTools/interface/ElectronEffectiveArea.h"
#include "LJMet/Com/interface/TopElectronSelector.h"

#include "DataFormats/JetReco/interface/CATopJetTagInfo.h"

using std::cout;
using std::endl;

class LjmetFactory;

class singleLepCalc : public BaseCalc {
public:
    singleLepCalc();
    virtual ~singleLepCalc();
    virtual int BeginJob();
    virtual int AnalyzeEvent(edm::EventBase const & event, BaseEventSelector * selector);
    virtual int EndJob(){return 0;};
    
private:
    edm::InputTag rhoSrc_;
    edm::InputTag muonSrc_;
    bool isWJets_;
    bool isTB_;
    bool isTT_;
    bool                      isMc;
    std::string               dataType;
//    edm::InputTag             rhoSrc_it;
    edm::InputTag             triggerSummary_;
    edm::InputTag             triggerCollection_;
    edm::InputTag             pvCollection_it;
    edm::InputTag             genParticles_it;
    std::vector<unsigned int> keepPDGID;
    std::vector<unsigned int> keepMomPDGID;
    bool keepFullMChistory;

    double rhoIso;

    std::vector<reco::Vertex> goodPVs;
    int findMatch(const reco::GenParticleCollection & genParticles, int idToMatch, double eta, double phi);
    double mdeltaR(double eta1, double phi1, double eta2, double phi2);
    void fillMotherInfo(const reco::Candidate *mother, int i, vector <int> & momid, vector <int> & momstatus, vector<double> & mompt, vector<double> & mometa, vector<double> & momphi, vector<double> & momenergy);


};

static int reg = LjmetFactory::GetInstance()->Register(new singleLepCalc(), "singleLepCalc");

singleLepCalc::singleLepCalc()
{
}

singleLepCalc::~singleLepCalc()
{
}

int singleLepCalc::BeginJob()
{
    if (mPset.exists("dataType"))     dataType = mPset.getParameter<std::string>("dataType");
    else                              dataType = "None"; 

    if (mPset.exists("rhoSrc")) rhoSrc_ = mPset.getParameter<edm::InputTag>("rhoSrc");
    else                        rhoSrc_ = edm::InputTag("fixedGridRhoAll");

    if (mPset.exists("pvCollection")) pvCollection_it = mPset.getParameter<edm::InputTag>("pvCollection");
    else                              pvCollection_it = edm::InputTag("offlineSlimmedPrimaryVertices");

    if (mPset.exists("triggerSummary")) triggerSummary_ = mPset.getParameter<edm::InputTag>("triggerSummary");
    else                                triggerSummary_ = edm::InputTag("selectedPatTrigger");
    
    if (mPset.exists("triggerCollection")) triggerCollection_ = mPset.getParameter<edm::InputTag>("triggerCollection");
    else                                   triggerCollection_ = edm::InputTag("TriggerResults::HLT");
    
    if (mPset.exists("isMc"))         isMc = mPset.getParameter<bool>("isMc");
    else                              isMc = false;

    if (mPset.exists("isTB"))    isTB_ = mPset.getParameter<bool>("isTB");
    else                         isTB_ = false;
    
    if (mPset.exists("isTT"))    isTT_ = mPset.getParameter<bool>("isTT");
    else                         isTT_ = false;
    
    if (mPset.exists("isWJets")) isWJets_ = mPset.getParameter<bool>("isWJets");
    else                         isWJets_ = false;

    if (mPset.exists("genParticles")) genParticles_it = mPset.getParameter<edm::InputTag>("genParticles");
    else                              genParticles_it = edm::InputTag("prunedGenParticles");

    if (mPset.exists("keepPDGID"))    keepPDGID = mPset.getParameter<std::vector<unsigned int> >("keepPDGID");
    else                              keepPDGID.clear();

    if (mPset.exists("keepMomPDGID")) keepMomPDGID = mPset.getParameter<std::vector<unsigned int> >("keepMomPDGID");
    else                              keepMomPDGID.clear();

    if (mPset.exists("keepFullMChistory")) keepFullMChistory = mPset.getParameter<bool>("keepFullMChistory");
    else                                   keepFullMChistory = true;
    cout << "keepFullMChistory "     <<    keepFullMChistory << endl;
 
    return 0;
}

int singleLepCalc::AnalyzeEvent(edm::EventBase const & event, BaseEventSelector * selector)
{     // ----- Get objects from the selector -----
    std::vector<edm::Ptr<pat::Jet> >            const & vSelJets = selector->GetSelectedJets();
    std::vector<edm::Ptr<pat::Jet> >            const & vSelBtagJets = selector->GetSelectedBtagJets();
    std::vector<edm::Ptr<pat::Jet> >            const & vAllJets = selector->GetAllJets();
    std::vector<std::pair<TLorentzVector,bool>> const & vCorrBtagJets = selector->GetCorrJetsWithBTags();
    std::vector<edm::Ptr<pat::Muon> >           const & vSelMuons = selector->GetSelectedMuons();
    std::vector<edm::Ptr<pat::Electron> >       const & vSelElectrons = selector->GetSelectedElectrons();
    edm::Ptr<pat::MET>                          const & pMet = selector->GetMet();
    std::vector<unsigned int>             const & vSelTriggers  = selector->GetSelectedTriggers();    
    // ----- Event kinematics -----
    int _nAllJets        = (int)vAllJets.size();
    int _nSelJets        = (int)vSelJets.size();
    int _nSelBtagJets    = (int)vSelBtagJets.size();
    int _nCorrBtagJets   = (int)vCorrBtagJets.size();
    int _nSelMuons       = (int)vSelMuons.size();
    int _nSelElectrons   = (int)vSelElectrons.size();
    edm::Handle<std::vector<reco::Vertex> > pvHandle;
    event.getByLabel(pvCollection_it, pvHandle);
    goodPVs = *(pvHandle.product());

    SetValue("nPV", (int)goodPVs.size());


 
     // _____ Primary dataset (from python cfg) _____________________
     //
     //   
    int dataE  = 0;
    int dataM  = 0;
 
    if      (dataType == "E" or dataType == "Electron") dataE = 1;
    else if (dataType == "M" or dataType == "Muon") dataM = 1;
    else if (dataType == "All" or dataType == "ALL") {
        dataE = 1; dataM = 1; 
    }

    SetValue("dataE", dataE);
    SetValue("dataM", dataM);


 
    // Trigger
    int passE = 0;
    int passM = 0;

    if (vSelTriggers.size() == 2) {
        passE = (int)vSelTriggers.at(0);
        passM = (int)vSelTriggers.at(1);
    }

 
    // Muon
   
    
    
    std::vector <int> muCharge;
    std::vector <int> muGlobal;
    //Four vector
    std::vector <double> muPt;
    std::vector <double> muEta;
    std::vector <double> muPhi;
    std::vector <double> muEnergy;
    //Quality criteria
    std::vector <double> muChi2;
    std::vector <double> muDxy;
    std::vector <double> muDz;
    std::vector <double> muRelIso;

    std::vector <int> muNValMuHits;
    std::vector <int> muNMatchedStations;
    std::vector <int> muNValPixelHits;
    std::vector <int> muNTrackerLayers;
    //Extra info about isolation
    std::vector <double> muChIso;
    std::vector <double> muNhIso;
    std::vector <double> muGIso;
    std::vector <double> muPuIso;
    //ID info
    std::vector <int> muIsTight;
    std::vector<int> muIsLoose;

    //Generator level information -- MC matching
    vector<double> muGen_Reco_dr;
    vector<int> muPdgId;
    vector<int> muStatus;
    vector<int> muMatched;
    vector<int> muNumberOfMothers;
    vector<double> muMother_pt;
    vector<double> muMother_eta;
    vector<double> muMother_phi;
    vector<double> muMother_energy;
    vector<int> muMother_id;
    vector<int> muMother_status;
    //Matched gen muon information:
    vector<double> muMatchedPt;
    vector<double> muMatchedEta;
    vector<double> muMatchedPhi;
    vector<double> muMatchedEnergy;

    for (std::vector<edm::Ptr<pat::Muon> >::const_iterator imu = vSelMuons.begin(); imu != vSelMuons.end(); imu++) 
        //Protect against muons without tracks (should never happen, but just in case)
        if ((*imu)->globalTrack().isNonnull() and (*imu)->globalTrack().isAvailable() and
            (*imu)->innerTrack().isNonnull()  and (*imu)->innerTrack().isAvailable()){


            //charge
            muCharge.push_back((*imu)->charge());
            // 4-vector 
            muPt     . push_back((*imu)->pt());
            muEta    . push_back((*imu)->eta());
            muPhi    . push_back((*imu)->phi());
            muEnergy . push_back((*imu)->energy());

            muIsTight.push_back((*imu)->isTightMuon(goodPVs.at(0)));
            muIsLoose.push_back((*imu)->isLooseMuon());

            muGlobal.push_back(((*imu)->isGlobalMuon()<<2)+(*imu)->isTrackerMuon());
            //chi2
            muChi2 . push_back((*imu)->globalTrack()->normalizedChi2());
            //Isolation
            double chIso  = (*imu)->userIsolation(pat::PfChargedHadronIso);
            double nhIso  = (*imu)->userIsolation(pat::PfNeutralHadronIso);
            double gIso   = (*imu)->userIsolation(pat::PfGammaIso);
            double puIso  = (*imu)->userIsolation(pat::PfPUChargedHadronIso);
            double relIso = (chIso + std::max(0.,nhIso + gIso - 0.5*puIso)) / (*imu)->pt();
            muRelIso . push_back(relIso);

            muChIso . push_back(chIso);
            muNhIso . push_back(nhIso);
            muGIso  . push_back(gIso);
            muPuIso . push_back(puIso);
            //IP: for some reason this is with respect to the first vertex in the collection
            if (goodPVs.size() > 0){
                muDxy . push_back((*imu)->muonBestTrack()->dxy(goodPVs.at(0).position()));
                muDz  . push_back((*imu)->muonBestTrack()->dz(goodPVs.at(0).position()));
            } else {
                muDxy . push_back(-999);
                muDz  . push_back(-999);
            }
            //Numbers of hits
            muNValMuHits       . push_back((*imu)->globalTrack()->hitPattern().numberOfValidMuonHits());
            muNMatchedStations . push_back((*imu)->numberOfMatchedStations());
            muNValPixelHits    . push_back((*imu)->innerTrack()->hitPattern().numberOfValidPixelHits());
            muNTrackerLayers   . push_back((*imu)->innerTrack()->hitPattern().trackerLayersWithMeasurement());
            if(isMc && keepFullMChistory){
                edm::Handle<reco::GenParticleCollection> genParticles;
                event.getByLabel(genParticles_it, genParticles);
                int matchId = findMatch(*genParticles, 13, (*imu)->eta(), (*imu)->phi());
                double closestDR = 10000.;
                if (matchId>=0) {
                    const reco::GenParticle & p = (*genParticles).at(matchId);
                    closestDR = mdeltaR( (*imu)->eta(), (*imu)->phi(), p.eta(), p.phi());
                    if(closestDR < 0.3){
                        muGen_Reco_dr.push_back(closestDR);
                        muPdgId.push_back(p.pdgId());
                        muStatus.push_back(p.status());
                        muMatched.push_back(1);
                        muMatchedPt.push_back( p.pt());
                        muMatchedEta.push_back(p.eta());
                        muMatchedPhi.push_back(p.phi());
                        muMatchedEnergy.push_back(p.energy());
                        int oldSize = muMother_id.size();
                        fillMotherInfo(p.mother(), 0, muMother_id, muMother_status, muMother_pt, muMother_eta, muMother_phi, muMother_energy);
                        muNumberOfMothers.push_back(muMother_id.size()-oldSize);
                    }
                }
                if(closestDR >= 0.3){
                    muNumberOfMothers.push_back(-1);
                    muGen_Reco_dr.push_back(-1.0);
                    muPdgId.push_back(-1);
                    muStatus.push_back(-1);
                    muMatched.push_back(0);
                    muMatchedPt.push_back(-1000.0);
                    muMatchedEta.push_back(-1000.0);
                    muMatchedPhi.push_back(-1000.0);
                    muMatchedEnergy.push_back(-1000.0);
                }
            }
        }
     // }
    SetValue("muCharge", muCharge);
    SetValue("muGlobal", muGlobal);
    SetValue("muPt"     , muPt);
    SetValue("muEta"    , muEta);
    SetValue("muPhi"    , muPhi);
    SetValue("muEnergy" , muEnergy);
    SetValue("muIsTight", muIsTight);
    SetValue("muIsLoose",muIsLoose); 
    //Quality criteria
    SetValue("muChi2"   , muChi2);
    SetValue("muDxy"    , muDxy);
    SetValue("muDz"     , muDz);
    SetValue("muRelIso" , muRelIso);

    SetValue("muNValMuHits"       , muNValMuHits);
    SetValue("muNMatchedStations" , muNMatchedStations);
    SetValue("muNValPixelHits"    , muNValPixelHits);
    SetValue("muNTrackerLayers"   , muNTrackerLayers);
    SetValue("muChIso", muChIso);
    SetValue("muNhIso", muNhIso);
    SetValue("muGIso" , muGIso);
    SetValue("muPuIso", muPuIso);
    //MC matching -- mother information
    SetValue("muGen_Reco_dr", muGen_Reco_dr);
    SetValue("muPdgId", muPdgId);
    SetValue("muStatus", muStatus);
    SetValue("muMatched",muMatched);
    SetValue("muMother_pt", muMother_pt);
    SetValue("muMother_eta", muMother_eta);
    SetValue("muMother_phi", muMother_phi);
    SetValue("muMother_energy", muMother_energy);
    SetValue("muMother_status", muMother_status);
    SetValue("muMother_id", muMother_id);
    SetValue("muNumberOfMothers", muNumberOfMothers);
    //Matched gen muon information:
    SetValue("muMatchedPt", muMatchedPt);
    SetValue("muMatchedEta", muMatchedEta);
    SetValue("muMatchedPhi", muMatchedPhi);
    SetValue("muMatchedEnergy", muMatchedEnergy);




    // Electron
    //Four vector
    std::vector <double> elPt;
    std::vector <double> elEta;
    std::vector <double> elPhi;
    std::vector <double> elEnergy;

    //Quality criteria
    std::vector <double> elRelIso;
    std::vector <double> elDxy;
    std::vector <int>    elNotConversion;
    std::vector <int>    elChargeConsistent;
    std::vector <int>    elIsEBEE;
    std::vector <int>    elCharge;

    //ID requirement
    std::vector <double> elDeta;
    std::vector <double> elDphi;
    std::vector <double> elSihih;
    std::vector <double> elHoE;
    std::vector <double> elD0;
    std::vector <double> elDZ;
    std::vector <double> elOoemoop;
    std::vector <int>    elMHits;
    std::vector <int>    elVtxFitConv;    

    //Extra info about isolation
    std::vector <double> elChIso;
    std::vector <double> elNhIso;
    std::vector <double> elPhIso;
    std::vector <double> elAEff;
    std::vector <double> elRhoIso;

    //mother-information
    //Generator level information -- MC matching
    vector<double> elGen_Reco_dr;
    vector<int> elPdgId;
    vector<int> elStatus;
    vector<int> elMatched;
    vector<int> elNumberOfMothers;
    vector<double> elMother_pt;
    vector<double> elMother_eta;
    vector<double> elMother_phi;
    vector<double> elMother_energy;
    vector<int> elMother_id;
    vector<int> elMother_status;
    //Matched gen electron information:
    vector<double> elMatchedPt;
    vector<double> elMatchedEta;
    vector<double> elMatchedPhi;
    vector<double> elMatchedEnergy;

 
    edm::Handle<double> rhoHandle;
    event.getByLabel(rhoSrc_, rhoHandle);
    double rhoIso = std::max(*(rhoHandle.product()), 0.0);
    //
    //_____Electrons______
    //

    for (std::vector<edm::Ptr<pat::Electron> >::const_iterator iel = vSelElectrons.begin(); iel != vSelElectrons.end(); iel++){
        //Protect against electrons without tracks (should never happen, but just in case)
        if ((*iel)->gsfTrack().isNonnull() and (*iel)->gsfTrack().isAvailable()){
            //Four vector
            elPt     . push_back((*iel)->ecalDrivenMomentum().pt()); //Must check: why ecalDrivenMomentum?
            elEta    . push_back((*iel)->ecalDrivenMomentum().eta());
            elPhi    . push_back((*iel)->ecalDrivenMomentum().phi());
            elEnergy . push_back((*iel)->ecalDrivenMomentum().energy());


            //Isolation
            double AEff  = ElectronEffectiveArea::GetElectronEffectiveArea(ElectronEffectiveArea::kEleGammaAndNeutralHadronIso03,
                                                                           (*iel)->superCluster()->eta(), ElectronEffectiveArea::kEleEAData2012);
            double chIso = (*iel)->chargedHadronIso();
            double nhIso = (*iel)->neutralHadronIso();
            double phIso = (*iel)->photonIso();
            double relIso = ( chIso + max(0.0, nhIso + phIso - rhoIso*AEff) ) / (*iel)->pt();

            elChIso  . push_back(chIso);
            elNhIso  . push_back(nhIso);
            elPhIso  . push_back(phIso);
            elAEff   . push_back(AEff);
            elRhoIso . push_back(rhoIso);

            elRelIso . push_back(relIso);
            //Conversion rejection
            int nLostHits = (*iel)->gsfTrack()->hitPattern().numberOfLostHits(reco::HitPattern::MISSING_INNER_HITS);
            double dist   = (*iel)->convDist();
            double dcot   = (*iel)->convDcot();
            int notConv   = nLostHits == 0 and (fabs(dist) > 0.02 or fabs(dcot) > 0.02);
            elCharge.push_back((*iel)->charge());
            elNotConversion.push_back(notConv);

            //IP: for some reason this is with respect to the first vertex in the collection
            if(goodPVs.size() > 0){
                elDxy.push_back((*iel)->gsfTrack()->dxy(goodPVs.at(0).position()));
                elDZ.push_back((*iel)->gsfTrack()->dz(goodPVs.at(0).position()));
            } else {
                elDxy.push_back(-999);
                elDZ.push_back(-999);
            }
            elChargeConsistent.push_back((*iel)->isGsfCtfScPixChargeConsistent());
            elIsEBEE.push_back(((*iel)->isEBEEGap()<<2) + ((*iel)->isEE()<<1) + (*iel)->isEB());
            elDeta.push_back((*iel)->deltaEtaSuperClusterTrackAtVtx());
            elDphi.push_back((*iel)->deltaPhiSuperClusterTrackAtVtx());
            elSihih.push_back((*iel)->sigmaIetaIeta());
            elHoE.push_back((*iel)->hadronicOverEm());
            elD0.push_back((*iel)->dB());
            elOoemoop.push_back(1.0/(*iel)->ecalEnergy() + (*iel)->eSuperClusterOverP()/(*iel)->ecalEnergy());
            elMHits.push_back((*iel)->gsfTrack()->hitPattern().numberOfHits(reco::HitPattern::MISSING_INNER_HITS));
            elVtxFitConv.push_back((*iel)->passConversionVeto());
            if(isMc && keepFullMChistory){
                //cout << "start\n";
                edm::Handle<reco::GenParticleCollection> genParticles;
                event.getByLabel(genParticles_it, genParticles);
                int matchId = findMatch(*genParticles, 11, (*iel)->eta(), (*iel)->phi());
                double closestDR = 10000.;
                //cout << "matchId "<<matchId <<endl;
                if (matchId>=0) {
                    const reco::GenParticle & p = (*genParticles).at(matchId);
                    closestDR = mdeltaR( (*iel)->eta(), (*iel)->phi(), p.eta(), p.phi());
                    //cout << "closestDR "<<closestDR <<endl;
                    if(closestDR < 0.3){
                        elGen_Reco_dr.push_back(closestDR);
                        elPdgId.push_back(p.pdgId());
                        elStatus.push_back(p.status());
                        elMatched.push_back(1);
                        elMatchedPt.push_back( p.pt());
                        elMatchedEta.push_back(p.eta());
                        elMatchedPhi.push_back(p.phi());
                        elMatchedEnergy.push_back(p.energy());
                        int oldSize = elMother_id.size();
                        fillMotherInfo(p.mother(), 0, elMother_id, elMother_status, elMother_pt, elMother_eta, elMother_phi, elMother_energy);
                        elNumberOfMothers.push_back(elMother_id.size()-oldSize);
                    }
                }
                if(closestDR >= 0.3){
                    elNumberOfMothers.push_back(-1);
                    elGen_Reco_dr.push_back(-1.0);
                    elPdgId.push_back(-1);
                    elStatus.push_back(-1);
                    elMatched.push_back(0);
                    elMatchedPt.push_back(-1000.0);
                    elMatchedEta.push_back(-1000.0);
                    elMatchedPhi.push_back(-1000.0);
                    elMatchedEnergy.push_back(-1000.0);

                }
            }//closing the isMC checking criteria
        }
    }

    //Four vector
    SetValue("elPt"     , elPt);
    SetValue("elEta"    , elEta);
    SetValue("elPhi"    , elPhi);
    SetValue("elEnergy" , elEnergy);

    SetValue("elCharge", elCharge);
    //Quality requirements
    SetValue("elRelIso" , elRelIso); //Isolation
    SetValue("elDxy"    , elDxy);    //Dxy
    SetValue("elNotConversion" , elNotConversion);  //Conversion rejection
    SetValue("elChargeConsistent", elChargeConsistent);
    SetValue("elIsEBEE", elIsEBEE);

    //ID cuts
    SetValue("elDeta", elDeta);
    SetValue("elDphi", elDphi);
    SetValue("elSihih", elSihih);
    SetValue("elHoE", elHoE);
    SetValue("elD0", elD0);
    SetValue("elDZ", elDZ);
    SetValue("elOoemoop", elOoemoop);
    SetValue("elMHits", elMHits);
    SetValue("elVtxFitConv", elVtxFitConv);

    //Extra info about isolation
    SetValue("elChIso" , elChIso);
    SetValue("elNhIso" , elNhIso);
    SetValue("elPhIso" , elPhIso);
    SetValue("elAEff"  , elAEff);
    SetValue("elRhoIso", elRhoIso);

    //MC matching -- mother information
    SetValue("elNumberOfMothers", elNumberOfMothers);
    SetValue("elGen_Reco_dr", elGen_Reco_dr);
    SetValue("elPdgId", elPdgId);
    SetValue("elStatus", elStatus);
    SetValue("elMatched",elMatched);
    SetValue("elMother_pt", elMother_pt);
    SetValue("elMother_eta", elMother_eta);
    SetValue("elMother_phi", elMother_phi);
    SetValue("elMother_energy", elMother_energy);
    SetValue("elMother_status", elMother_status);
    SetValue("elMother_id", elMother_id);
    //Matched gen muon information:
    SetValue("elMatchedPt", elMatchedPt);
    SetValue("elMatchedEta", elMatchedEta);
    SetValue("elMatchedPhi", elMatchedPhi);
    SetValue("elMatchedEnergy", elMatchedEnergy);

    //
    //______Trigger Matching __________________
    //

    edm::Handle<edm::TriggerResults > mhEdmTriggerResults;
    event.getByLabel( triggerCollection_ , mhEdmTriggerResults );
    edm::Handle<pat::TriggerObjectStandAloneCollection> mhEdmTriggerObjectColl;  
    event.getByLabel(triggerSummary_,mhEdmTriggerObjectColl);

    const edm::TriggerNames &names = event.triggerNames(*mhEdmTriggerResults);

    int _electron_1_hltmatched =0;
    int _muon_1_hltmatched =0;

    if (_nSelElectrons>0 || _nSelMuons>0) {
        for(pat::TriggerObjectStandAlone obj : *mhEdmTriggerObjectColl){       
            obj.unpackPathNames(names);
            for(unsigned h = 0; h < obj.filterLabels().size(); ++h){
		if ( _nSelElectrons>0 ){
                    if ( obj.filterLabels()[h]!="hltEle32WP85GsfTrackIsoFilter" ) continue;
  	            if ( deltaR(obj.eta(),obj.phi(),elEta[0],elPhi[0]) < 0.5 ) _electron_1_hltmatched = 1;
		}
		if ( _nSelMuons>0 ){
                    if ( obj.filterLabels()[h]!="hltL3crIsoL1sMu20Eta2p1L1f0L2f20QL3f24QL3crIsoRhoFiltered0p15IterTrk02" ) continue;
  	            if ( deltaR(obj.eta(),obj.phi(),muEta[0],muPhi[0]) < 0.5 ) _muon_1_hltmatched = 1;
		}
            }
        }
    }

    SetValue("electron_1_hltmatched",_electron_1_hltmatched);
    SetValue("muon_1_hltmatched",_muon_1_hltmatched);


    //
    //_____ Jets ______________________________
    //

       
    //Get all AK8 jets (not just for W and Top)
    edm::InputTag AK8JetColl = edm::InputTag("slimmedJetsAK8");
    edm::Handle<std::vector<pat::Jet> > AK8Jets;
    event.getByLabel(AK8JetColl, AK8Jets);

    //Four vector
    std::vector <double> AK8JetPt;
    std::vector <double> AK8JetEta;
    std::vector <double> AK8JetPhi;
    std::vector <double> AK8JetEnergy;

    std::vector <double> AK8JetCSV;
    //   std::vector <double> AK8JetRCN;       
    for (std::vector<pat::Jet>::const_iterator ijet = AK8Jets->begin(); ijet != AK8Jets->end(); ijet++){

        TLorentzVector lvak8 = selector->correctJet(*ijet, event,true);
        //Four vector
        AK8JetPt     . push_back(lvak8.Pt());
        AK8JetEta    . push_back(lvak8.Eta());
        AK8JetPhi    . push_back(lvak8.Phi());
        AK8JetEnergy . push_back(lvak8.Energy());

        AK8JetCSV    . push_back(ijet->bDiscriminator( "combinedInclusiveSecondaryVertexV2BJetTags"));
        //     AK8JetRCN    . push_back((ijet->chargedEmEnergy()+ijet->chargedHadronEnergy()) / (ijet->neutralEmEnergy()+ijet->neutralHadronEnergy()));
    }
 
    //Four vector
    SetValue("AK8JetPt"     , AK8JetPt);
    SetValue("AK8JetEta"    , AK8JetEta);
    SetValue("AK8JetPhi"    , AK8JetPhi);
    SetValue("AK8JetEnergy" , AK8JetEnergy);

    SetValue("AK8JetCSV"    , AK8JetCSV);
    //   SetValue("AK8JetRCN"    , AK8JetRCN);
    //Get AK4 Jets
    //Four vector
    std::vector <double> AK4JetPt;
    std::vector <double> AK4JetEta;
    std::vector <double> AK4JetPhi;
    std::vector <double> AK4JetEnergy;

    std::vector <int>    AK4JetBTag;
    std::vector <double> AK4JetRCN;   
    double AK4HT =.0;
    for (std::vector<edm::Ptr<pat::Jet> >::const_iterator ijet = vSelJets.begin();
         ijet != vSelJets.end(); ijet++){

        //Four vector
        TLorentzVector lv = selector->correctJet(**ijet, event);

        AK4JetPt     . push_back(lv.Pt());
        AK4JetEta    . push_back(lv.Eta());
        AK4JetPhi    . push_back(lv.Phi());
        AK4JetEnergy . push_back(lv.Energy());
        
        AK4JetBTag   . push_back(selector->isJetTagged(**ijet, event));
        AK4JetRCN    . push_back(((*ijet)->chargedEmEnergy()+(*ijet)->chargedHadronEnergy()) / ((*ijet)->neutralEmEnergy()+(*ijet)->neutralHadronEnergy()));
 
        //HT
        AK4HT += lv.Pt(); 
    }
    
    //Four vector
    SetValue("AK4JetPt"     , AK4JetPt);
    SetValue("AK4JetEta"    , AK4JetEta);
    SetValue("AK4JetPhi"    , AK4JetPhi);
    SetValue("AK4JetEnergy" , AK4JetEnergy);
    SetValue("AK4HT"        , AK4HT);
    SetValue("AK4JetBTag"   , AK4JetBTag);
    SetValue("AK4JetRCN"    , AK4JetRCN);

    // MET
    double _met = -9999.0;
    double _met_phi = -9999.0;
    // Corrected MET
    double _corr_met = -9999.0;
    double _corr_met_phi = -9999.0;

    if(pMet.isNonnull() && pMet.isAvailable()) {
        _met = pMet->p4().pt();
        _met_phi = pMet->p4().phi();

        TLorentzVector corrMET = selector->correctMet(*pMet, event);
        if(corrMET.Pt()>0) {
            _corr_met = corrMET.Pt();
            _corr_met_phi = corrMET.Phi();
        }

    }
    SetValue("met", _met);
    SetValue("met_phi", _met_phi);
    SetValue("corr_met", _corr_met);
    SetValue("corr_met_phi", _corr_met_phi);

    //_____ Gen Info ______________________________
    //

    //Four vector
    std::vector <double> genPt;
    std::vector <double> genEta;
    std::vector <double> genPhi;
    std::vector <double> genEnergy;

    //Identity
    std::vector <int> genID;
    std::vector <int> genIndex;
    std::vector <int> genStatus;
    std::vector <int> genMotherID;
    std::vector <int> genMotherIndex;

    if (isMc){
        edm::Handle<reco::GenParticleCollection> genParticles;
        event.getByLabel(genParticles_it, genParticles);

        for(size_t i = 0; i < genParticles->size(); i++){
            const reco::GenParticle & p = (*genParticles).at(i);

            //Find status 23 particles
            if (p.status() == 23){
                reco::Candidate* mother = (reco::Candidate*) p.mother();
                if (not mother)            continue;

                bool bKeep = false;
                for (unsigned int uk = 0; uk < keepMomPDGID.size(); uk++){
                    if (abs(mother->pdgId()) == (int) keepMomPDGID.at(uk)){
                        bKeep = true;
                        break;
                    }
                }

                if (not bKeep){
                    for (unsigned int uk = 0; uk < keepPDGID.size(); uk++){
                        if (abs(p.pdgId()) == (int) keepPDGID.at(uk)){
                            bKeep = true;
                            break;
                        }
                    }
                }

                if (not bKeep) continue;

                //Find index of mother
                int mInd = 0;
                for(size_t j = 0; j < genParticles->size(); j++){
                    const reco::GenParticle & q = (*genParticles).at(j);
                    if (q.status() != 3) continue;
                    if (mother->pdgId() == q.pdgId() and fabs(mother->eta() - q.eta()) < 0.01 and fabs(mother->pt() - q.pt()) < 0.01){
                        mInd = (int) j;
                        break;
                    }
                }

                //Four vector
                genPt     . push_back(p.pt());
                genEta    . push_back(p.eta());
                genPhi    . push_back(p.phi());
                genEnergy . push_back(p.energy());

                //Identity
                genID            . push_back(p.pdgId());
                genIndex         . push_back((int) i);
                genStatus        . push_back(p.status());
                genMotherID      . push_back(mother->pdgId());
                genMotherIndex   . push_back(mInd);
            }
        }//End loop over gen particles
    }  //End MC-only if
    // Four vector
    SetValue("genPt"    , genPt);
    SetValue("genEta"   , genEta);
    SetValue("genPhi"   , genPhi);
    SetValue("genEnergy", genEnergy);

    // Identity
    SetValue("genID"         , genID);
    SetValue("genIndex"      , genIndex);
    SetValue("genStatus"     , genStatus);
    SetValue("genMotherID"   , genMotherID);
    SetValue("genMotherIndex", genMotherIndex);



    return 0;
}

int singleLepCalc::findMatch(const reco::GenParticleCollection & genParticles, int idToMatch, double eta, double phi)
{
    float dRtmp = 1000;
    float closestDR = 10000.;
    int closestGenPart = -1;

    for(size_t j = 0; j < genParticles.size(); ++ j) {
        const reco::GenParticle & p = (genParticles).at(j);
        dRtmp = mdeltaR( eta, phi, p.eta(), p.phi());
        if ( dRtmp < closestDR && abs(p.pdgId()) == idToMatch){// && dRtmp < 0.3) {
            closestDR = dRtmp;
            closestGenPart = j;
        }//end of requirement for matching
    }//end of gen particle loop 
    return closestGenPart;
}


double singleLepCalc::mdeltaR(double eta1, double phi1, double eta2, double phi2) {
    return std::sqrt(deltaR2 (eta1, phi1, eta2, phi2));
}

void singleLepCalc::fillMotherInfo(const reco::Candidate *mother, int i, vector <int> & momid, vector <int> & momstatus, vector<double> & mompt, vector<double> & mometa, vector<double> & momphi, vector<double> & momenergy)
{
    if(mother) {
        momid.push_back(mother->pdgId());
        momstatus.push_back(mother->status());
        mompt.push_back(mother->pt());
        mometa.push_back(mother->eta());
        momphi.push_back(mother->phi());
        momenergy.push_back(mother->energy());
        if(i<10)fillMotherInfo(mother->mother(), i+1, momid, momstatus, mompt, mometa, momphi, momenergy);
    }


}





