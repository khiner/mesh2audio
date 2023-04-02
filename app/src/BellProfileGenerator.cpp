#include "BellProfileGenerator.h"

float smoothstep(float edge0, float edge1, float x) {
    const float t = glm::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

vec2 cubic_bezier(float t, const vec2 &P0, const vec2 &P1, const vec2 &P2, const vec2 &P3) {
    const float u = 1 - t;
    const float tt = t * t;
    const float uu = u * u;
    return (uu * u * P0) + (3 * uu * t * P1) + (3 * u * tt * P2) + (tt * t * P3);
}

vector<vec2> GenerateBellProfile(float H, float D, float g, float r, float phi, int num_points, float a, float b, vec2 P1, vec2 P2) {
    vector<vec2> profile;
    const float R = r * D / 2;
    const float t = g * R;
    vec2 tmp;

    for (int i = 0; i < 2 * num_points; ++i) {
        const bool is_outer = i < num_points;
        const int idx = is_outer ? i : 2 * num_points - i - 1;
        const float x = -R + 2 * R * idx / float(num_points);

        float y1, y2, y3;
        if (is_outer) {
            y1 = b * sqrt(1 - (x * x) / (a * a));
            y2 = cubic_bezier(x, vec2(-R, 0), P1, P2, vec2(R, 0)).y;
            y3 = a * cosh(x / a);
        } else {
            y1 = b * sqrt(1 - ((x - t * cos(phi)) * (x - t * cos(phi))) / (a * a));
            tmp = vec2(t * cos(phi), t * sin(phi));
            y2 = cubic_bezier(x, vec2(-R, 0) + tmp, P1 - tmp, P2 - tmp, vec2(R - tmp.x, tmp.y)).y;
            y3 = a * cosh((x - t * cos(phi)) / a);
        }

        const float blend_head_shoulder = smoothstep(-R, -R / 4, x);
        const float blend_waist = smoothstep(-R / 4, R / 4, x);
        const float blend_sound_bow_lip = smoothstep(R / 4, R, x);

        const float y = y1 * blend_head_shoulder + y2 * blend_waist + y3 * blend_sound_bow_lip;
        profile.emplace_back(x, y);
    }

    // Normalize profile so that all x values are positive.
    float min_x = INFINITY;
    for (const auto &point : profile) {
        if (point.x < min_x) min_x = point.x;
    }
    for (auto &point : profile) {
        point.x -= min_x; // Shift x values to be positive.
        point.y *= H; // Scale by height
    }

    return profile;
}

vector<vec2> GenerateBellProfile() {
    const int num_points = 100;
    const float H = 0.8f, D = 1.0f, g = 0.1f, r = 0.8f, a = 0.8f, b = 1.0f, phi = glm::radians(10.0f);
    const float R = r * D / 2;
    const vec2 P1(-0.3f, 0.3f), P2(0.3f, 0.3f);
    const float waist_height = 4 * cubic_bezier(0.5f, vec2(-R, 0), P1, P2, vec2(R, 0)).y;
    const float height_scale = H / waist_height;
    return GenerateBellProfile(H, D, g, r, phi, num_points, a, b, {P1.x, P1.y * height_scale}, {P2.x, P2.y * height_scale});
}
