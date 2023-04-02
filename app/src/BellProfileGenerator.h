#include <glm/glm.hpp>
#include <vector>

using std::vector, glm::vec2;

vector<vec2> GenerateBellProfile();
vector<vec2> GenerateBellProfile(float H, float D, float g, float r, float phi, int num_points, float a, float b, glm::vec2 P1, glm::vec2 P2);
