#include <metal_stdlib>
#import "ShaderTypes.h"
using namespace metal;

struct Vertex {
    float3 position [[attribute(VertexAttributePosition)]];
    float3 normal [[attribute(VertexAttributeNormal)]];
};

struct RasterOut {
    float4 position [[position]];
    float3 normal;
    float3 worldPosition;
    float4 color;
    float pointSize [[point_size]];
};

vertex RasterOut meshVertex(Vertex in [[stage_in]],
                            constant Uniforms& uniforms [[buffer(BufferIndexUniforms)]]) {
    RasterOut out;
    float4 world = uniforms.model * float4(in.position, 1.0);
    out.position = uniforms.viewProjection * world;
    if (uniforms.counts.y == 1u) {
        float4 lensClip = uniforms.viewProjection * float4(uniforms.positionRadius.xyz, 1.0);
        if (lensClip.w > 0.0 && out.position.w > lensClip.w + 0.001) {
            float2 lensNDC = lensClip.xy / lensClip.w;
            float2 sourceNDC = out.position.xy / out.position.w;
            float2 beta = sourceNDC - lensNDC;
            float betaLength = length(beta);
            float depthRatio = (out.position.w - lensClip.w) / out.position.w;
            float thetaE = 0.12 * sqrt(max(uniforms.lensParameters.x, 0.0) * depthRatio /
                                       max(lensClip.w, 1.0));
            float imageRadius = 0.5 * (betaLength + sqrt(betaLength * betaLength + 4.0 * thetaE * thetaE));
            if (betaLength > 1.0e-5) {
                out.position.xy = (lensNDC + beta * (imageRadius / betaLength)) * out.position.w;
            }
        }
    }
    out.worldPosition = world.xyz;
    out.normal = normalize((uniforms.model * float4(in.normal, 0.0)).xyz);
    out.color = uniforms.colorMass;
    out.pointSize = 1.0;
    return out;
}

static float random01(uint value, uint seed) {
    uint x = value * 747796405u + seed * 2891336453u + 277803737u;
    x = ((x >> ((x >> 28u) + 4u)) ^ x) * 277803737u;
    x = (x >> 22u) ^ x;
    return float(x) / 4294967295.0;
}

static float3 heatColor(float value) {
    float3 cold = float3(0.12, 0.015, 0.35);
    float3 red = float3(0.85, 0.04, 0.025);
    float3 gold = float3(1.0, 0.54, 0.035);
    float3 white = float3(1.0, 0.96, 0.78);
    if (value < 0.34) return mix(cold, red, value / 0.34);
    if (value < 0.72) return mix(red, gold, (value - 0.34) / 0.38);
    return mix(gold, white, (value - 0.72) / 0.28);
}

static float sampleHydrogen1sRadius(float u) {
    float x = max(pow(6.0 * u, 1.0 / 3.0), 0.001);
    for (uint i = 0; i < 6; ++i) {
        float e = exp(-x);
        float cdf = 1.0 - e * (1.0 + x + 0.5 * x * x);
        float derivative = 0.5 * x * x * e;
        x = max(x - (cdf - u) / max(derivative, 1.0e-6), 0.001);
    }
    return x;
}

vertex RasterOut particleVertex(uint id [[vertex_id]],
                                constant Uniforms& uniforms [[buffer(BufferIndexUniforms)]]) {
    RasterOut out;
    uint mode = uniforms.counts.y;
    float a = random01(id, 11u);
    float b = random01(id, 29u);
    float c = random01(id, 47u);
    float radialRandom = random01(id, 61u);
    float theta = 6.28318530718 * a;
    float cosPhi = 2.0 * b - 1.0;
    float sinPhi = sqrt(max(1.0 - cosPhi * cosPhi, 0.0));
    float3 direction = float3(cos(theta) * sinPhi, cosPhi, sin(theta) * sinPhi);
    float3 position;
    float heat;

    if (mode == 2u) {
        uint electronCount = max(uniforms.counts.w, 1u);
        float shell = 1.0 + floor(pow(c, 0.42) * min(float(electronCount), 8.0));
        float protonCount = max(uniforms.colorMass.w, 1.0);
        float effectiveCharge = max(protonCount - 0.72 * max(shell - 1.0, 0.0), 1.0);
        float radial = 0.24 * (shell == 1.0 ? sampleHydrogen1sRadius(radialRandom) :
                              shell * (0.5 + 2.0 * pow(radialRandom, 0.72))) / effectiveCharge;
        position = direction * radial;
        heat = exp(-radial * effectiveCharge / max(shell, 0.5)) *
               (1.0 + 0.18 * float(electronCount));
        out.pointSize = clamp(float(uniforms.counts.z) * 0.1, 1.0, 12.0);
    } else {
        float horizon = max(uniforms.sceneParameters.z, 0.08);
        float radius = horizon * 1.65 + c * (uniforms.positionRadius.w * 0.88);
        float angularVelocity = uniforms.sceneParameters.x * uniforms.sceneParameters.y * uniforms.colorMass.w /
                                max(radius * radius, 0.03);
        float trailOffset = uniforms.lensParameters.w * 0.08;
        float phase = theta + angularVelocity - trailOffset * uniforms.sceneParameters.y *
                      uniforms.colorMass.w / max(radius * radius, 0.03);
        position = uniforms.positionRadius.xyz + float3(cos(phase) * radius,
                    (b - 0.5) * 0.10, sin(phase) * radius);
        heat = 1.0 - smoothstep(horizon * 1.7, horizon * 8.0, radius);
        out.pointSize = 3.0;
    }
    out.position = uniforms.viewProjection * float4(position, 1.0);
    out.worldPosition = position;
    out.normal = direction;
    float alpha = mode == 2u ? 0.8 : 0.6 * exp(-uniforms.lensParameters.w * 0.75);
    out.color = float4(heatColor(clamp(heat, 0.0, 1.0)), alpha);
    return out;
}

vertex RasterOut gridVertex(uint id [[vertex_id]],
                            constant Uniforms& uniforms [[buffer(BufferIndexUniforms)]]) {
    RasterOut out;
    float spacing = max(uniforms.sceneParameters.w, 0.1);
    float extent = max(uniforms.positionRadius.w, 1.0);
    uint lines = uint(2.0 * extent / spacing) + 1u;
    uint segments = 96u;
    uint verticesPerLine = segments * 2u;
    uint line = id / verticesPerLine;
    uint withinLine = id % verticesPerLine;
    uint segment = withinLine / 2u;
    float t = (float(segment) + float(withinLine % 2u)) / float(segments);
    float coordinate = (float(line) - 0.5 * float(lines - 1u)) * spacing;
    float x = (line % 2u == 0u) ? mix(-extent, extent, t) : coordinate;
    float z = (line % 2u == 0u) ? coordinate : mix(-extent, extent, t);
    float3 p = float3(x, 0.0, z);
    float3 delta = p - uniforms.positionRadius.xyz;
    float distance = length(delta) + max(uniforms.sceneParameters.z, 0.08);
    p.y -= min(3.8 * uniforms.colorMass.w * uniforms.sceneParameters.z / distance, 4.0);
    out.position = uniforms.viewProjection * float4(p, 1.0);
    out.worldPosition = p;
    out.normal = float3(0, 1, 0);
    out.color = float4(0.10, 0.29, 0.42, clamp(uniforms.lensParameters.w, 0.05, 1.0));
    out.pointSize = 1.0;
    return out;
}

fragment float4 sceneFragment(RasterOut in [[stage_in]]) {
    float lighting = 0.25 + 0.75 * max(dot(normalize(in.normal), normalize(float3(-0.4, 0.8, 0.6))), 0.0);
    float4 color = in.color;
    color.rgb *= lighting;
    return color;
}

fragment float4 particleFragment(RasterOut in [[stage_in]], float2 point [[point_coord]]) {
    float radius = length(point - 0.5);
    float alpha = (1.0 - smoothstep(0.22, 0.5, radius)) * in.color.a;
    return float4(in.color.rgb, alpha);
}

fragment float4 gridFragment(RasterOut in [[stage_in]]) { return in.color; }
