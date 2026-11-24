#pragma once
#include <map>
#include <osg/Group>
#include <osg/Program>
#include <osg/Uniform>
#include <string>
#include <utility>
#include <vector>

namespace Pyrive {
namespace OsgKit {
// Visual prototype only: assigned temperatures + a model-space Gaussian mask.
// Creates private node/Drawable/StateSet shells; source geometry/resources
// untouched.
class InfraredPreview {
public:
  enum class Mode { Visible = 0, WhiteHot = 1, BlackHot = 2, FalseColor = 3 };
  struct Parameters {
    float hullKelvin = 296, cabinKelvin = 301, minimumKelvin = 288,
          maximumKelvin = 333;
    osg::Vec3 heatCenter{0, 0, 0}, heatRadius{4, 6, 8};
    float heatDeltaKelvin = 0; // OFF unless explicitly configured by the host
    float surfaceVariationKelvin =
        2.5f; // estimated top-surface variation, not lighting
    std::map<std::string, float> nodeKelvin; // nearest named node wins
  };
  explicit InfraredPreview(osg::Node *source);
  osg::Node *node() const { return node_.get(); }
  void setMode(Mode mode);
  std::map<std::string, unsigned> setParameters(const Parameters &parameters);

private:
  osg::ref_ptr<osg::Node> node_;
  osg::ref_ptr<osg::Program> program_;
  std::vector<std::pair<osg::ref_ptr<osg::StateSet>, std::vector<std::string>>>
      states_;
};
} // namespace OsgKit
} // namespace Pyrive
