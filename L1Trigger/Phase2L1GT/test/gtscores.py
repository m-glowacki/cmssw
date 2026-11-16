
import FWCore.ParameterSet.Config as cms

from Configuration.Eras.Era_Phase2C17I13M9_cff import Phase2C17I13M9
from Configuration.AlCa.GlobalTag import GlobalTag

process = cms.Process("L1P2GT", Phase2C17I13M9)

process.load("Configuration.StandardSequences.Services_cff")
process.load("SimGeneral.HepPDTESSource.pythiapdt_cfi")
process.load("FWCore.MessageService.MessageLogger_cfi")
process.load("Configuration.EventContent.EventContent_cff")
process.load("SimGeneral.MixingModule.mixNoPU_cfi")
process.load("Configuration.Geometry.GeometryExtendedRun4D121Reco_cff")
process.load("Geometry.HcalTowerAlgo.HcalTrigTowerGeometryESProducer_cfi")
process.load("Configuration.StandardSequences.MagneticField_cff")
process.load("Configuration.StandardSequences.L1TrackTrigger_cff")
process.load("Configuration.StandardSequences.SimL1Emulator_cff")
process.load("Configuration.StandardSequences.SimPhase2L1GlobalTriggerEmulator_cff")
process.load("L1Trigger.Configuration.Phase2GTMenus.SeedDefinitions.step1_2024.l1tGTMenu_cff")
process.load("Configuration.StandardSequences.EndOfProcess_cff")
process.load("Configuration.StandardSequences.FrontierConditions_GlobalTag_cff")
process.load("CalibCalorimetry.CaloTPG.CaloTPGTranscoder_cfi")

process.GlobalTag = GlobalTag(
    process.GlobalTag,
    "auto:phase2_realistic_T35",
    ""
)

# The input is a MINIAOD: the PixelDigiSimLink collection produced by the
# mixing module (mix:Tracker) is not stored, but the DIGI-step copy is.
process.TTClusterAssociatorFromPixelDigis.digiSimLinks = cms.InputTag(
    "simSiPixelDigis", "Tracker"
)

process.source = cms.Source(
    "PoolSource",
    fileNames=cms.untracked.vstring(
        "file:/eos/cms/store/mc/Phase2Spring24DIGIRECOMiniAOD/TTTo2L2Nu_TuneCP5_14TeV-powheg-pythia8/GEN-SIM-DIGI-RAW-MINIAOD/PU200_Trk1GeV_140X_mcRun4_realistic_v4-v1/2810000/3b081cbc-6a4e-429a-a2d5-be0241900a26.root"
    ),
    inputCommands=cms.untracked.vstring(
        "keep *",
        "drop l1tPFJets_*_*_*",
        "drop l1tTrackerMuons_l1tTkMuonsGmt*_*_HLT"
    )
)

process.maxEvents = cms.untracked.PSet(
    input=cms.untracked.int32(10)
)

process.options = cms.untracked.PSet(
    wantSummary=cms.untracked.bool(True),
    numberOfThreads=cms.untracked.uint32(1),
    numberOfStreams=cms.untracked.uint32(0)
)

process.MessageLogger.cerr.FwkReport.reportEvery = 100

# Phase-2 L1 emulation paths
process.L1TrackTrigger_step = cms.Path(
    process.L1TrackTrigger
)

process.L1simulation_step = cms.Path(
    process.SimL1Emulator
)

process.Phase2L1GTProducer = cms.Path(
    process.l1tGTProducerSequence
)

# Model score producer
process.l1tGTScoreProducer = cms.EDProducer(
    "L1GTScoreProducer",
    modelPath=cms.string(
        "L1Trigger/Phase2L1GT/data/triggermodel/metadata.json"
    ),
    sortByPT=cms.bool(True),
    inputs=cms.VPSet(
        cms.PSet(
            tag=cms.InputTag("l1tGTProducer", "CL2JetsSC4"),
            nObjects=cms.uint32(4),
            features=cms.vstring(
                "hwPT_toInt",
                "hwEta_toInt",
                "hwPhi_toInt"
            )
        ),
        cms.PSet(
            tag=cms.InputTag("l1tGTProducer", "CL2EtSum"),
            nObjects=cms.uint32(1),
            features=cms.vstring(
                "hwPT_toInt",
                "hwPhi_toInt"
            )
        )
    )
)

process.pScore = cms.Path(
    process.l1tGTScoreProducer
)

# ROOT output containing the score product
process.scoreOutput = cms.OutputModule(
    "PoolOutputModule",
    fileName=cms.untracked.string("scores.root"),
    outputCommands=cms.untracked.vstring(
        "drop *",
        "keep *_l1tGTScoreProducer_*_*"
    ),
    dataset=cms.untracked.PSet(
        dataTier=cms.untracked.string("NANOAOD"),
        filterName=cms.untracked.string("")
    )
)

process.scoreOutput_step = cms.EndPath(
    process.scoreOutput
)

# Retain the standard end-of-process path
process.endjob_step = cms.EndPath(
    process.endOfProcess
)


process.schedule = cms.Schedule(
    process.L1TrackTrigger_step,
    process.L1simulation_step,
    process.Phase2L1GTProducer,
    process.pScore,
    process.endjob_step,
    process.scoreOutput_step
)
