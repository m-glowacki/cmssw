

#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/Framework/interface/stream/EDProducer.h"
#include "FWCore/ParameterSet/interface/ConfigurationDescriptions.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h"
#include "FWCore/Utilities/interface/FileInPath.h"

#include "DataFormats/L1Trigger/interface/P2GTCandidate.h"

#include "CommonTools/Utils/interface/StringObjectFunction.h"
#include "hls4ml/emulator.h"

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <algorithm>
#include <any>
#include <memory>
#include <numeric>
#include <string>
#include <vector>

using namespace l1t;

namespace {

  struct InputSpec {
    edm::InputTag tag;
    edm::EDGetTokenT<P2GTCandidateCollection> token;
    unsigned int nObjects = 1;
    // one entry per configured feature; "valid" is handled by the presence flag
    std::vector<StringObjectFunction<P2GTCandidate>> getters;
    std::vector<bool> presence;
  };

  struct EmulatorModel {
    std::unique_ptr<hls4mlEmulator::ModelLoader> loader;
    std::shared_ptr<hls4mlEmulator::Model> model;
    unsigned int nOutputs = 0;

    void load(const std::string& path, unsigned int nOutputs_) {
      // ModelLoader appends the ".so" suffix itself
      std::string libName = path;
      const std::string suffix = ".so";
      if (libName.size() > suffix.size() &&
          libName.compare(libName.size() - suffix.size(), suffix.size(), suffix) == 0)
        libName.resize(libName.size() - suffix.size());
      loader = std::make_unique<hls4mlEmulator::ModelLoader>(libName);
      model = loader->load_model();
      nOutputs = nOutputs_;
    }

    std::vector<float> evaluate(std::vector<float>& input) {
      std::vector<float> output(nOutputs, 0.f);
      model->prepare_input(std::any(input.data()));
      model->predict();
      model->read_result(std::any(output.data()));
      return output;
    }
  };

}  // namespace

class L1GTScoreProducer : public edm::stream::EDProducer<> {
public:
  explicit L1GTScoreProducer(const edm::ParameterSet&);
  ~L1GTScoreProducer() override = default;

  static void fillDescriptions(edm::ConfigurationDescriptions&);

private:
  void produce(edm::Event&, const edm::EventSetup&) override;

  EmulatorModel emu_;
  bool sortByPT_;
  StringObjectFunction<P2GTCandidate> pT_;
  std::vector<InputSpec> inputs_;
  unsigned int nFeatures_ = 0;
};

L1GTScoreProducer::L1GTScoreProducer(const edm::ParameterSet& config)
    : sortByPT_(config.getParameter<bool>("sortByPT")), pT_("hwPT_toInt") {
  const std::string metadataPath = edm::FileInPath(config.getParameter<std::string>("modelPath")).fullPath();
  const std::string workspace = metadataPath.substr(0, metadataPath.rfind('/'));

  boost::property_tree::ptree metadata;
  boost::property_tree::read_json(metadataPath, metadata);
  const std::string name = metadata.get<std::string>("name");

  for (const edm::ParameterSet& inputPSet : config.getParameter<std::vector<edm::ParameterSet>>("inputs")) {
    InputSpec spec;
    spec.tag = inputPSet.getParameter<edm::InputTag>("tag");
    spec.token = consumes<P2GTCandidateCollection>(spec.tag);
    spec.nObjects = inputPSet.getParameter<unsigned int>("nObjects");
    for (const std::string& feature : inputPSet.getParameter<std::vector<std::string>>("features")) {
      spec.presence.push_back(feature == "valid");
      if (feature == "valid")
        spec.getters.emplace_back("hwPT_toInt");  // unused placeholder, guarded by presence
      else
        spec.getters.emplace_back(feature);
    }
    nFeatures_ += spec.nObjects * spec.getters.size();
    inputs_.push_back(std::move(spec));
  }

  emu_.load(workspace + "/emulator/" + name + "/" + name,
            metadata.get<unsigned int>("n_outputs", 1));

  produces<std::vector<float>>();
}

void L1GTScoreProducer::produce(edm::Event& event, const edm::EventSetup&) {
  std::vector<float> input(nFeatures_, 0.f);
  size_t pos = 0;

  for (const auto& spec : inputs_) {
    edm::Handle<P2GTCandidateCollection> handle;
    event.getByToken(spec.token, handle);

    std::vector<size_t> order(handle->size());
    std::iota(order.begin(), order.end(), size_t{0});
    if (sortByPT_)
      std::stable_sort(order.begin(), order.end(),
                       [&handle, this](size_t a, size_t b) { return pT_((*handle)[a]) > pT_((*handle)[b]); });

    for (size_t k = 0; k < spec.nObjects; ++k) {
      for (size_t f = 0; f < spec.getters.size(); ++f) {
        if (k >= order.size()) {
          input[pos++] = 0.f;  // 0-padded slot
        } else if (spec.presence[f]) {
          input[pos++] = 1.f;  // "valid": a real object is present
        } else {
          input[pos++] = spec.getters[f]((*handle)[order[k]]);
        }
      }
    }
  }

  event.put(std::make_unique<std::vector<float>>(emu_.evaluate(input)));
}

void L1GTScoreProducer::fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
  edm::ParameterSetDescription desc;
  desc.add<std::string>("modelPath")->setComment(
      "TriggerModel metadata.json (emulator/<name>/<name>.so sits next to it)");
  desc.add<bool>("sortByPT", true)->setComment("stable-sort every input collection by hw pT (descending)");
  edm::ParameterSetDescription inputDesc;
  inputDesc.add<edm::InputTag>("tag");
  inputDesc.add<unsigned int>("nObjects", 1);
  inputDesc.add<std::vector<std::string>>("features")->setComment(
      "StringObjectFunction expressions on P2GTCandidate (raw ints), or 'valid'");
  desc.addVPSet("inputs", inputDesc, std::vector<edm::ParameterSet>());
  descriptions.add("l1tGTScoreProducer", desc);
}

DEFINE_FWK_MODULE(L1GTScoreProducer);
