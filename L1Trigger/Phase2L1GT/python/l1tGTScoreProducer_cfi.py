import FWCore.ParameterSet.Config as cms

l1tGTScoreProducer = cms.EDProducer(
    "L1GTScoreProducer",
    modelPath=cms.string("L1Trigger/Phase2L1GT/data/triggermodel/metadata.json"),
    sortByPT=cms.bool(True),
    inputs=cms.VPSet(
        cms.PSet(
            tag=cms.InputTag("l1tGTProducer", "CL2JetsSC4"),
            nObjects=cms.uint32(4),
            features=cms.vstring("hwPT_toInt", "hwEta_toInt", "hwPhi_toInt"),
        ),
        cms.PSet(
            tag=cms.InputTag("l1tGTProducer", "CL2EtSum"),
            nObjects=cms.uint32(1),
            features=cms.vstring("hwPT_toInt", "hwPhi_toInt"),
        ),
    ),
)
