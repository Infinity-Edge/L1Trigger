// -*- C++ -*-
//
// Package: L1CaloTrigger
// Class: L1TowerCalibrator
//
/**\class L1TowerCalibrator L1TowerCalibrator.cc

Description: 
Take the calibrated unclustered ECAL energy and total HCAL
energy associated with the L1CaloTower collection output
from L1EGammaCrystalsEmulatorProducer: l1CaloTowerCollection, "L1CaloTowerCollection"

as well as HGCal Tower level inputs:
BXVector<l1t::HGCalTower> "hgcalTriggerPrimitiveDigiProducer" "tower" "HLT"     

and HCAL HF inputs from:
edm::SortedCollection<HcalTriggerPrimitiveDigi,edm::StrictWeakOrdering<HcalTriggerPrimitiveDigi> > "simHcalTriggerPrimitiveDigis" "" "HLT"     


Implement PU-based calibrations which scale down the ET
in the towers based on mapping nTowers with ECAL(HCAL) ET <= defined PU threshold.
This value has been shown to be similar between TTbar, QCD, and minBias samples.
This allows a prediction of nvtx. Which can be mapped to the total minBias
energy in an eta slice of the detector.  Subtract the total estimated minBias energy
per eta slice divided by nTowers in that eta slice from each trigger tower in
that eta slice.

This is all ECAL / HCAL specific or EM vs. Hadronic for HGCal.

Implementation:
[Notes on implementation]
*/
//
// Original Author: Tyler Ruggles
// Created: Thu Nov 15 2018
// $Id$
//
//


#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/Framework/interface/EDProducer.h"
#include "FWCore/Framework/interface/ESHandle.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/ServiceRegistry/interface/Service.h"
#include "CommonTools/UtilAlgos/interface/TFileService.h"

#include <iostream>

#include "DataFormats/Phase2L1CaloTrig/interface/L1CaloTower.h"
#include "DataFormats/L1THGCal/interface/HGCalTower.h"
#include "DataFormats/HcalDigi/interface/HcalDigiCollections.h"
#include "SimDataFormats/CaloHit/interface/PCaloHitContainer.h"

#include "CalibFormats/CaloTPG/interface/CaloTPGTranscoder.h"
#include "CalibFormats/CaloTPG/interface/CaloTPGRecord.h"

#include "L1Trigger/L1TCalorimeter/interface/CaloTools.h"

#include "TFile.h"
#include "TF1.h"


class L1TowerCalibrator : public edm::EDProducer {
    public:
        explicit L1TowerCalibrator(const edm::ParameterSet&);

    private:
        virtual void produce(edm::Event&, const edm::EventSetup&);

        double HcalTpEtMin;
        double EcalTpEtMin;
        double HGCalHadTpEtMin;
        double HGCalEmTpEtMin;
        double HFTpEtMin;
        double puThreshold;
        double puThresholdEcal;
        double puThresholdHcal;
        double puThresholdL1eg;
        double puThresholdHGCalEMMin;
        double puThresholdHGCalEMMax;
        double puThresholdHGCalHadMin;
        double puThresholdHGCalHadMax;
        double puThresholdHFMin;
        double puThresholdHFMax;
        double barrelSF;
        double hgcalSF;
        double hfSF;
        bool debug;
        bool skipCalibrations;

        edm::EDGetTokenT< L1CaloTowerCollection > l1TowerToken_;
        edm::Handle< L1CaloTowerCollection > l1CaloTowerHandle;

        edm::EDGetTokenT<l1t::HGCalTowerBxCollection> hgcalTowersToken_;
        edm::Handle<l1t::HGCalTowerBxCollection> hgcalTowersHandle;
        l1t::HGCalTowerBxCollection hgcalTowers;

        edm::EDGetTokenT<HcalTrigPrimDigiCollection> hcalToken_;
        edm::Handle<HcalTrigPrimDigiCollection> hcalTowerHandle;
        edm::ESHandle<CaloTPGTranscoder> decoder_;

        // nHits to nvtx functions
        std::vector<edm::ParameterSet> nHits_to_nvtx_params;
        std::map< std::string, TF1 > nHits_to_nvtx_funcs;

        // nvtx to PU subtraction functions
        std::vector<edm::ParameterSet> nvtx_to_PU_sub_params;
        std::map< std::string, TF1 > ecal_nvtx_to_PU_sub_funcs;
        std::map< std::string, TF1 > hcal_nvtx_to_PU_sub_funcs;
        std::map< std::string, TF1 > hgcalEM_nvtx_to_PU_sub_funcs;
        std::map< std::string, TF1 > hgcalHad_nvtx_to_PU_sub_funcs;
        std::map< std::string, TF1 > hf_nvtx_to_PU_sub_funcs;
        std::map< std::string, std::map< std::string, TF1 > > all_nvtx_to_PU_sub_funcs;

};

L1TowerCalibrator::L1TowerCalibrator(const edm::ParameterSet& iConfig) :
    HcalTpEtMin(iConfig.getParameter<double>("HcalTpEtMin")),
    EcalTpEtMin(iConfig.getParameter<double>("EcalTpEtMin")),
    HGCalHadTpEtMin(iConfig.getParameter<double>("HGCalHadTpEtMin")),
    HGCalEmTpEtMin(iConfig.getParameter<double>("HGCalEmTpEtMin")),
    HFTpEtMin(iConfig.getParameter<double>("HFTpEtMin")),
    puThreshold(iConfig.getParameter<double>("puThreshold")),
    puThresholdEcal(iConfig.getParameter<double>("puThresholdEcal")),
    puThresholdHcal(iConfig.getParameter<double>("puThresholdHcal")),
    puThresholdL1eg(iConfig.getParameter<double>("puThresholdL1eg")),
    puThresholdHGCalEMMin(iConfig.getParameter<double>("puThresholdHGCalEMMin")),
    puThresholdHGCalEMMax(iConfig.getParameter<double>("puThresholdHGCalEMMax")),
    puThresholdHGCalHadMin(iConfig.getParameter<double>("puThresholdHGCalHadMin")),
    puThresholdHGCalHadMax(iConfig.getParameter<double>("puThresholdHGCalHadMax")),
    puThresholdHFMin(iConfig.getParameter<double>("puThresholdHFMin")),
    puThresholdHFMax(iConfig.getParameter<double>("puThresholdHFMax")),
    barrelSF(iConfig.getParameter<double>("barrelSF")),
    hgcalSF(iConfig.getParameter<double>("hgcalSF")),
    hfSF(iConfig.getParameter<double>("hfSF")),
    debug(iConfig.getParameter<bool>("debug")),
    skipCalibrations(iConfig.getParameter<bool>("skipCalibrations")),
    l1TowerToken_(consumes< L1CaloTowerCollection >(iConfig.getParameter<edm::InputTag>("l1CaloTowers"))),
    hgcalTowersToken_(consumes<l1t::HGCalTowerBxCollection>(iConfig.getParameter<edm::InputTag>("L1HgcalTowersInputTag"))),
    hcalToken_(consumes<HcalTrigPrimDigiCollection>(iConfig.getParameter<edm::InputTag>("hcalDigis"))),
    nHits_to_nvtx_params(iConfig.getParameter< std::vector<edm::ParameterSet> >("nHits_to_nvtx_params")),
    nvtx_to_PU_sub_params(iConfig.getParameter< std::vector<edm::ParameterSet> >("nvtx_to_PU_sub_params"))
{


    // Initialize the nHits --> nvtx functions
    for ( uint i = 0; i < nHits_to_nvtx_params.size(); i++ )
    {
        edm::ParameterSet * pset = &nHits_to_nvtx_params.at(i);
        std::string calo = pset->getParameter< std::string >("fit");
        nHits_to_nvtx_funcs[ calo.c_str() ] = TF1( calo.c_str(), "[0] + [1] * x"); 
        nHits_to_nvtx_funcs[ calo.c_str() ].SetParameter( 0, pset->getParameter< std::vector<double> >("params").at(0) );
        nHits_to_nvtx_funcs[ calo.c_str() ].SetParameter( 1, pset->getParameter< std::vector<double> >("params").at(1) );

        if(debug)
        {
            printf("nHits_to_nvtx_params[%i]\n \
                fit: %s \n \
                p1: %f \n \
                p2 %f \n", i, calo.c_str(),
                nHits_to_nvtx_funcs[ calo.c_str() ].GetParameter( 0 ),
                nHits_to_nvtx_funcs[ calo.c_str() ].GetParameter( 1 ) );
        }
    }




    // Initialize the nvtx --> PU subtraction functions
    all_nvtx_to_PU_sub_funcs[ "ecal" ] =     ecal_nvtx_to_PU_sub_funcs;
    all_nvtx_to_PU_sub_funcs[ "hcal" ] =     hcal_nvtx_to_PU_sub_funcs;
    all_nvtx_to_PU_sub_funcs[ "hgcalEM" ] =  hgcalEM_nvtx_to_PU_sub_funcs;
    all_nvtx_to_PU_sub_funcs[ "hgcalHad" ] = hgcalHad_nvtx_to_PU_sub_funcs;
    all_nvtx_to_PU_sub_funcs[ "hf" ] =       hf_nvtx_to_PU_sub_funcs;

    for ( uint i = 0; i < nvtx_to_PU_sub_params.size(); i++ )
    {
        edm::ParameterSet * pset = &nvtx_to_PU_sub_params.at(i);
        std::string calo = pset->getParameter< std::string >("calo");
        std::string iEta = pset->getParameter< std::string >("iEta");
        double p1 = pset->getParameter< std::vector<double> >("params").at(0);
        double p2 = pset->getParameter< std::vector<double> >("params").at(1);

        all_nvtx_to_PU_sub_funcs[ calo.c_str() ][ iEta.c_str() ] = TF1( calo.c_str(), "[0] + [1] * x"); 
        all_nvtx_to_PU_sub_funcs[ calo.c_str() ][ iEta.c_str() ].SetParameter( 0, p1 );
        all_nvtx_to_PU_sub_funcs[ calo.c_str() ][ iEta.c_str() ].SetParameter( 1, p2 );

        if(debug)
        {
            printf("nvtx_to_PU_sub_params[%i]\n \
                sub detector: %s \n \
                iEta: %s \n \
                p1: %f \n \
                p2 %f \n", i, calo.c_str(), iEta.c_str(), p1, p2 );
        }
    }



    // Our two outputs, calibrated towers and estimated nvtx for fun
    produces< L1CaloTowerCollection >("L1CaloTowerCalibratedCollection");
    produces< double >("EstimatedNvtx");

}

void L1TowerCalibrator::produce(edm::Event& iEvent, const edm::EventSetup& iSetup)
{



    // Estimated number of vertices used for calibration estimattion
    std::unique_ptr< double > EstimatedNvtx(new double);
    // Calibrated output collection
    std::unique_ptr< L1CaloTowerCollection > L1CaloTowerCalibratedCollection(new L1CaloTowerCollection);
    





    // Load the ECAL+HCAL tower sums coming from L1EGammaCrystalsEmulatorProducer.cc
    iEvent.getByToken(l1TowerToken_,l1CaloTowerHandle);

    // HGCal info
    iEvent.getByToken(hgcalTowersToken_,hgcalTowersHandle);
    hgcalTowers = (*hgcalTowersHandle.product());

    // HF Tower info
    iEvent.getByToken(hcalToken_,hcalTowerHandle);
    
    // Barrel ECAL (unclustered) and HCAL
    for (auto& hit : *l1CaloTowerHandle.product())
    {

        L1CaloTower l1Hit;
        l1Hit.ecal_tower_et  = hit.ecal_tower_et;
        l1Hit.hcal_tower_et  = hit.hcal_tower_et;
        // Add min ET thresholds for tower ET
        if (l1Hit.ecal_tower_et < EcalTpEtMin) l1Hit.ecal_tower_et = 0.0;
        if (l1Hit.hcal_tower_et < HcalTpEtMin) l1Hit.hcal_tower_et = 0.0;
        l1Hit.tower_iEta  = hit.tower_iEta;
        l1Hit.tower_iPhi  = hit.tower_iPhi;
        l1Hit.tower_eta  = hit.tower_eta;
        l1Hit.tower_phi  = hit.tower_phi;

        // FIXME There is an error in the L1EGammaCrystalsEmulatorProducer.cc which is
        // returning towers with minimal ECAL energy, and no HCAL energy with these
        // iEta/iPhi coordinates and eta = -88.653152 and phi = -99.000000.
        // Skip these for the time being until the upstream code has been debugged
        if ((int)l1Hit.tower_iEta == -1016 && (int)l1Hit.tower_iPhi == -962) continue;


        (*L1CaloTowerCalibratedCollection).push_back( l1Hit );
        if (debug) printf("Barrel tower iEta %i iPhi %i eta %f phi %f ecal_et %f hcal_et_sum %f\n", (int)l1Hit.tower_iEta, (int)l1Hit.tower_iPhi, l1Hit.tower_eta, l1Hit.tower_phi, l1Hit.ecal_tower_et, l1Hit.hcal_tower_et);
    }

    // Loop over HGCalTowers and create L1CaloTowers for them and add to collection
    // This iterator is taken from the PF P2 group
    // https://github.com/p2l1pfp/cmssw/blob/170808db68038d53794bc65fdc962f8fc337a24d/L1Trigger/Phase2L1ParticleFlow/plugins/L1TPFCaloProducer.cc#L278-L289
    for (auto it = hgcalTowers.begin(0), ed = hgcalTowers.end(0); it != ed; ++it)
    {
        // skip lowest ET towers
        if (it->etEm() < HGCalEmTpEtMin && it->etHad() < HGCalHadTpEtMin) continue;

        L1CaloTower l1Hit;
        // Set energies normally, but need to zero if below threshold
        if (it->etEm() < HGCalEmTpEtMin) l1Hit.ecal_tower_et  = 0.;
        else l1Hit.ecal_tower_et  = it->etEm();

        if (it->etHad() < HGCalHadTpEtMin) l1Hit.hcal_tower_et  = 0.;
        else l1Hit.hcal_tower_et  = it->etHad();

        l1Hit.tower_eta  = it->eta();
        l1Hit.tower_phi  = it->phi();
        l1Hit.tower_iEta  = -98; // -98 mean HGCal
        l1Hit.tower_iPhi  = -98; 
        (*L1CaloTowerCalibratedCollection).push_back( l1Hit );
        if (debug) printf("HGCal tower iEta %i iPhi %i eta %f phi %f ecal_et %f hcal_et_sum %f\n", (int)l1Hit.tower_iEta, (int)l1Hit.tower_iPhi, l1Hit.tower_eta, l1Hit.tower_phi, l1Hit.ecal_tower_et, l1Hit.hcal_tower_et);
    }


    // Loop over Hcal HF tower inputs and create L1CaloTowers and add to
    // L1CaloTowerCalibratedCollection collection
    iSetup.get<CaloTPGRecord>().get(decoder_);
    for (const auto & hit : *hcalTowerHandle.product()) {
        HcalTrigTowerDetId id = hit.id();
        double et = decoder_->hcaletValue(hit.id(), hit.t0());
        if (et < HFTpEtMin) continue;
        // Only doing HF so skip outside range
        if ( abs(id.ieta()) < l1t::CaloTools::kHFBegin ) continue;
        if ( abs(id.ieta()) > l1t::CaloTools::kHFEnd ) continue;

        L1CaloTower l1Hit;
        l1Hit.ecal_tower_et  = 0.;
        l1Hit.hcal_tower_et  = et;
        l1Hit.tower_eta  = l1t::CaloTools::towerEta(id.ieta());
        l1Hit.tower_phi  = l1t::CaloTools::towerPhi(id.ieta(), id.iphi());
        l1Hit.tower_iEta  = id.ieta();
        l1Hit.tower_iPhi  = id.iphi();
        (*L1CaloTowerCalibratedCollection).push_back( l1Hit );

        if (debug) printf("HCAL HF tower iEta %i iPhi %i eta %f phi %f ecal_et %f hcal_et_sum %f\n", (int)l1Hit.tower_iEta, (int)l1Hit.tower_iPhi, l1Hit.tower_eta, l1Hit.tower_phi, l1Hit.ecal_tower_et, l1Hit.hcal_tower_et);
    }



    // N Tower totals
    // For mapping to estimated nvtx in event
    int i_ecal_hits_leq_threshold = 0;
    int i_hgcalEM_hits_leq_threshold = 0;
    int i_hcal_hits_leq_threshold = 0;
    int i_hgcalHad_hits_leq_threshold = 0;
    int i_hf_hits_leq_threshold = 0;


    // Loop over the collection containing all hits
    // and calculate the number of hits falling into the 
    // "less than or equal" nTowers variable which maps to
    // estimated number of vertices
    for (auto &l1CaloTower : (*L1CaloTowerCalibratedCollection) )
    {


        // Barrel ECAL
        if(l1CaloTower.ecal_tower_et > 0. && l1CaloTower.tower_iEta != -98) 
        {
            if(l1CaloTower.ecal_tower_et <= puThresholdEcal) 
            {
                i_ecal_hits_leq_threshold++;
            }
        }


        // HGCal EM
        if(l1CaloTower.ecal_tower_et > 0. && l1CaloTower.tower_iEta == -98) 
        {
            if(l1CaloTower.ecal_tower_et <= puThresholdHGCalEMMax && l1CaloTower.ecal_tower_et >= puThresholdHGCalEMMin) 
            {
                i_hgcalEM_hits_leq_threshold++;
            }
        }


        // Barrel HCAL
        if(l1CaloTower.hcal_tower_et > 0. && l1CaloTower.tower_iEta != -98 && abs(l1CaloTower.tower_eta) < 2.0) // abs(eta) < 2 just keeps us out of HF 
        {
            if(l1CaloTower.hcal_tower_et <= puThresholdHcal)
            {
                i_hcal_hits_leq_threshold++;
            }
        }


        // HGCal Had
        if(l1CaloTower.hcal_tower_et > 0. && l1CaloTower.tower_iEta == -98) 
        {
            if(l1CaloTower.hcal_tower_et <= puThresholdHGCalHadMax && l1CaloTower.hcal_tower_et >= puThresholdHGCalHadMin) 
            {
                i_hgcalHad_hits_leq_threshold++;
            }
        }

        // HF
        if(l1CaloTower.hcal_tower_et > 0. && l1CaloTower.tower_iEta != -98 && abs(l1CaloTower.tower_eta) > 2.0) // abs(eta) > 2 keeps us out of barrel HF 
        {
            if(l1CaloTower.hcal_tower_et <= puThresholdHFMax && l1CaloTower.hcal_tower_et >= puThresholdHFMin)
            {
                i_hf_hits_leq_threshold++;
            }
        }
    }



    // For each subdetector, map to the estimated number of vertices
    double ecal_nvtx = nHits_to_nvtx_funcs[ "ecal" ].Eval( i_ecal_hits_leq_threshold );
    double hcal_nvtx = nHits_to_nvtx_funcs[ "hcal" ].Eval( i_hcal_hits_leq_threshold );
    double hgcalEM_nvtx = nHits_to_nvtx_funcs[ "hgcalEM" ].Eval( i_hgcalEM_hits_leq_threshold );
    double hgcalHad_nvtx = nHits_to_nvtx_funcs[ "hgcalHad" ].Eval( i_hgcalHad_hits_leq_threshold );
    double hf_nvtx = nHits_to_nvtx_funcs[ "hf" ].Eval( i_hf_hits_leq_threshold );
    // Make sure all values are >= 0
    if (ecal_nvtx < 0) ecal_nvtx = 0;
    if (hcal_nvtx < 0) hcal_nvtx = 0;
    if (hgcalEM_nvtx < 0) hgcalEM_nvtx = 0;
    if (hgcalHad_nvtx < 0) hgcalHad_nvtx = 0;
    if (hf_nvtx < 0) hf_nvtx = 0;
    // Best estimate is avg of all except HF.
    // This is b/c HF currently has such poor prediction power, it only degrades the avg result
    *EstimatedNvtx = ( ecal_nvtx + hcal_nvtx + hgcalEM_nvtx + hgcalHad_nvtx ) / 4.;




    if(debug)
    {
        double lumi = iEvent.eventAuxiliary().luminosityBlock();
        double event = iEvent.eventAuxiliary().event();

        printf("L1TowerCalibrater: lumi %.0f evt %.0f nTowers for subdetecters \
            \nECAL:      %i --> nvtx = %.1f \
            \nHGCal EM:  %i --> nvtx = %.1f \
            \nHCAL:      %i --> nvtx = %.1f \
            \nHGCal Had: %i --> nvtx = %.1f \
            \nHCAL HF:   %i --> nvtx = %.1f \
            \nEstimated Nvtx = %.1f\n", lumi, event, 
                i_ecal_hits_leq_threshold, ecal_nvtx,
                i_hgcalEM_hits_leq_threshold, hgcalEM_nvtx, 
                i_hcal_hits_leq_threshold, hcal_nvtx, 
                i_hgcalHad_hits_leq_threshold, hgcalHad_nvtx,
                i_hf_hits_leq_threshold, hf_nvtx,
                *EstimatedNvtx );
    }




    // Use estimated number of vertices to subtract off PU contributions
    // to each and every hit. In cases where the energy would go negative,
    // limit this to zero.
    if (!skipCalibrations) // skipCalibrations simply passes the towers through
    {
        for (auto &l1CaloTower : (*L1CaloTowerCalibratedCollection) )
        {

            // Barrel ECAL eta slices
            if(l1CaloTower.ecal_tower_et > 0. && l1CaloTower.tower_iEta != -98) 
            {
                if( abs(l1CaloTower.tower_iEta) <= 3 )
                { 
                    l1CaloTower.ecal_tower_et -= all_nvtx_to_PU_sub_funcs[ "ecal" ][ "er1to3" ].Eval( *EstimatedNvtx ) * barrelSF;
                }
                if( abs(l1CaloTower.tower_iEta) <= 6 && abs(l1CaloTower.tower_iEta) >= 4 )
                { 
                    l1CaloTower.ecal_tower_et -= all_nvtx_to_PU_sub_funcs[ "ecal" ][ "er4to6" ].Eval( *EstimatedNvtx ) * barrelSF;
                }
                if( abs(l1CaloTower.tower_iEta) <= 9 && abs(l1CaloTower.tower_iEta) >= 7 )
                { 
                    l1CaloTower.ecal_tower_et -= all_nvtx_to_PU_sub_funcs[ "ecal" ][ "er7to9" ].Eval( *EstimatedNvtx ) * barrelSF;
                }
                if( abs(l1CaloTower.tower_iEta) <= 12 && abs(l1CaloTower.tower_iEta) >= 10 )
                { 
                    l1CaloTower.ecal_tower_et -= all_nvtx_to_PU_sub_funcs[ "ecal" ][ "er10to12" ].Eval( *EstimatedNvtx ) * barrelSF;
                }
                if( abs(l1CaloTower.tower_iEta) <= 15 && abs(l1CaloTower.tower_iEta) >= 13 )
                { 
                    l1CaloTower.ecal_tower_et -= all_nvtx_to_PU_sub_funcs[ "ecal" ][ "er13to15" ].Eval( *EstimatedNvtx ) * barrelSF;
                }
                if( abs(l1CaloTower.tower_iEta) <= 18 && abs(l1CaloTower.tower_iEta) >= 16 )
                { 
                    l1CaloTower.ecal_tower_et -= all_nvtx_to_PU_sub_funcs[ "ecal" ][ "er16to18" ].Eval( *EstimatedNvtx ) * barrelSF;
                }
            }


            // HGCal EM eta slices
            if(l1CaloTower.ecal_tower_et > 0. && l1CaloTower.tower_iEta == -98) 
            {
                if( abs(l1CaloTower.tower_eta) <= 1.8 )
                { 
                    l1CaloTower.ecal_tower_et -= all_nvtx_to_PU_sub_funcs[ "hgcalEM" ][ "er1p4to1p8" ].Eval( *EstimatedNvtx ) * hgcalSF;
                }
                if( abs(l1CaloTower.tower_eta) <= 2.1 && abs(l1CaloTower.tower_eta) > 1.8 )
                { 
                    l1CaloTower.ecal_tower_et -= all_nvtx_to_PU_sub_funcs[ "hgcalEM" ][ "er1p8to2p1" ].Eval( *EstimatedNvtx ) * hgcalSF;
                }
                if( abs(l1CaloTower.tower_eta) <= 2.4 && abs(l1CaloTower.tower_eta) > 2.1 )
                { 
                    l1CaloTower.ecal_tower_et -= all_nvtx_to_PU_sub_funcs[ "hgcalEM" ][ "er2p1to2p4" ].Eval( *EstimatedNvtx ) * hgcalSF;
                }
                if( abs(l1CaloTower.tower_eta) <= 2.7 && abs(l1CaloTower.tower_eta) > 2.4 )
                { 
                    l1CaloTower.ecal_tower_et -= all_nvtx_to_PU_sub_funcs[ "hgcalEM" ][ "er2p4to2p7" ].Eval( *EstimatedNvtx ) * hgcalSF;
                }
                if( abs(l1CaloTower.tower_eta) <= 3.1 && abs(l1CaloTower.tower_eta) > 2.7 )
                { 
                    l1CaloTower.ecal_tower_et -= all_nvtx_to_PU_sub_funcs[ "hgcalEM" ][ "er2p7to3p1" ].Eval( *EstimatedNvtx ) * hgcalSF;
                }
            }

            // Barrel HCAL eta slices
            if(l1CaloTower.hcal_tower_et > 0. && l1CaloTower.tower_iEta != -98 && abs(l1CaloTower.tower_eta) < 2.0) // abs(eta) < 2 just keeps us out of HF 
            {
                if( abs(l1CaloTower.tower_iEta) <= 3 )
                { 
                    l1CaloTower.hcal_tower_et -= all_nvtx_to_PU_sub_funcs[ "hcal" ][ "er1to3" ].Eval( *EstimatedNvtx ) * barrelSF;
                }
                if( abs(l1CaloTower.tower_iEta) <= 6 && abs(l1CaloTower.tower_iEta) >= 4 )
                { 
                    l1CaloTower.hcal_tower_et -= all_nvtx_to_PU_sub_funcs[ "hcal" ][ "er4to6" ].Eval( *EstimatedNvtx ) * barrelSF;
                }
                if( abs(l1CaloTower.tower_iEta) <= 9 && abs(l1CaloTower.tower_iEta) >= 7 )
                { 
                    l1CaloTower.hcal_tower_et -= all_nvtx_to_PU_sub_funcs[ "hcal" ][ "er7to9" ].Eval( *EstimatedNvtx ) * barrelSF;
                }
                if( abs(l1CaloTower.tower_iEta) <= 12 && abs(l1CaloTower.tower_iEta) >= 10 )
                { 
                    l1CaloTower.hcal_tower_et -= all_nvtx_to_PU_sub_funcs[ "hcal" ][ "er10to12" ].Eval( *EstimatedNvtx ) * barrelSF;
                }
                if( abs(l1CaloTower.tower_iEta) <= 15 && abs(l1CaloTower.tower_iEta) >= 13 )
                { 
                    l1CaloTower.hcal_tower_et -= all_nvtx_to_PU_sub_funcs[ "hcal" ][ "er13to15" ].Eval( *EstimatedNvtx ) * barrelSF;
                }
                if( abs(l1CaloTower.tower_iEta) <= 18 && abs(l1CaloTower.tower_iEta) >= 16 )
                { 
                    l1CaloTower.hcal_tower_et -= all_nvtx_to_PU_sub_funcs[ "hcal" ][ "er16to18" ].Eval( *EstimatedNvtx ) * barrelSF;
                }
            }

            // HGCal Had eta slices
            if(l1CaloTower.hcal_tower_et > 0. && l1CaloTower.tower_iEta == -98) 
            {
                if( abs(l1CaloTower.tower_eta) <= 1.8 )
                { 
                    l1CaloTower.hcal_tower_et -= all_nvtx_to_PU_sub_funcs[ "hgcalHad" ][ "er1p4to1p8" ].Eval( *EstimatedNvtx ) * hgcalSF;
                }
                if( abs(l1CaloTower.tower_eta) <= 2.1 && abs(l1CaloTower.tower_eta) > 1.8 )
                { 
                    l1CaloTower.hcal_tower_et -= all_nvtx_to_PU_sub_funcs[ "hgcalHad" ][ "er1p8to2p1" ].Eval( *EstimatedNvtx ) * hgcalSF;
                }
                if( abs(l1CaloTower.tower_eta) <= 2.4 && abs(l1CaloTower.tower_eta) > 2.1 )
                { 
                    l1CaloTower.hcal_tower_et -= all_nvtx_to_PU_sub_funcs[ "hgcalHad" ][ "er2p1to2p4" ].Eval( *EstimatedNvtx ) * hgcalSF;
                }
                if( abs(l1CaloTower.tower_eta) <= 2.7 && abs(l1CaloTower.tower_eta) > 2.4 )
                { 
                    l1CaloTower.hcal_tower_et -= all_nvtx_to_PU_sub_funcs[ "hgcalHad" ][ "er2p4to2p7" ].Eval( *EstimatedNvtx ) * hgcalSF;
                }
                if( abs(l1CaloTower.tower_eta) <= 3.1 && abs(l1CaloTower.tower_eta) > 2.7 )
                { 
                    l1CaloTower.hcal_tower_et -= all_nvtx_to_PU_sub_funcs[ "hgcalHad" ][ "er2p7to3p1" ].Eval( *EstimatedNvtx ) * hgcalSF;
                }
            }

            // HF eta slices
            if(l1CaloTower.hcal_tower_et > 0. && l1CaloTower.tower_iEta != -98 && abs(l1CaloTower.tower_eta) > 2.0) // abs(eta) > 2 keeps us out of barrel HF 
            {
                if( abs(l1CaloTower.tower_iEta) <= 33 && abs(l1CaloTower.tower_iEta) >= 29 )
                { 
                    l1CaloTower.hcal_tower_et -= all_nvtx_to_PU_sub_funcs[ "hf" ][ "er29to33" ].Eval( *EstimatedNvtx ) * hfSF;
                }
                if( abs(l1CaloTower.tower_iEta) <= 37 && abs(l1CaloTower.tower_iEta) >= 34 )
                { 
                    l1CaloTower.hcal_tower_et -= all_nvtx_to_PU_sub_funcs[ "hf" ][ "er34to37" ].Eval( *EstimatedNvtx ) * hfSF;
                }
                if( abs(l1CaloTower.tower_iEta) <= 41 && abs(l1CaloTower.tower_iEta) >= 38 )
                { 
                    l1CaloTower.hcal_tower_et -= all_nvtx_to_PU_sub_funcs[ "hf" ][ "er38to41" ].Eval( *EstimatedNvtx ) * hfSF;
                }
            }


            // Make sure none are negative
            if(l1CaloTower.ecal_tower_et < 0.) l1CaloTower.ecal_tower_et = 0.;
            if(l1CaloTower.hcal_tower_et < 0.) l1CaloTower.hcal_tower_et = 0.;
        }
    }



    iEvent.put(std::move(EstimatedNvtx),"EstimatedNvtx");
    iEvent.put(std::move(L1CaloTowerCalibratedCollection),"L1CaloTowerCalibratedCollection");
}



DEFINE_FWK_MODULE(L1TowerCalibrator);
