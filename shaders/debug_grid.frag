#version 450

layout(push_constant) uniform PushConstants {
    mat4 inverseViewProjection;
    vec4 cameraPosition;
    vec4 gridSettings;
    vec4 fadeSettings;
} pushConstants;

layout(location = 0) out vec4 outColor;

const vec4 MinorColor = vec4(0.27, 0.34, 0.43, 0.16);
const vec4 MajorColor = vec4(0.40, 0.50, 0.62, 0.30);
const vec4 AxisXColor = vec4(0.95, 0.28, 0.28, 0.65);
const vec4 AxisYColor = vec4(0.30, 0.85, 0.45, 0.65);

// Grid is fully visible when camera height is below this.
const float HeightFadeStart = 250.0;

// Grid fully disappears when camera height reaches this.
const float HeightFadeEnd = 1200.0;

vec3 reconstructWorld(vec2 uv, float depth)
{
    vec2 ndc = vec2(uv.x * 2.0 - 1.0, uv.y * 2.0 - 1.0);
    vec4 world = pushConstants.inverseViewProjection * vec4(ndc, depth, 1.0);
    return world.xyz / max(abs(world.w), 0.00001);
}

float gridLine(vec2 worldPosition, float step)
{
    vec2 grid = worldPosition / max(step, 0.0001);
    vec2 width = max(fwidth(grid), vec2(0.0001));
    vec2 distanceToLine = abs(fract(grid - 0.5) - 0.5) / width;
    return 1.0 - clamp(min(distanceToLine.x, distanceToLine.y), 0.0, 1.0);
}

float axisLine(float coordinate)
{
    float width = max(fwidth(coordinate), 0.0001);
    return 1.0 - clamp(abs(coordinate) / width, 0.0, 1.0);
}

float radialFadeProgress(vec3 worldPosition)
{
    float fadeStart = pushConstants.fadeSettings.x;
    float fadeEnd = max(pushConstants.fadeSettings.y, fadeStart + 0.001);

    // Grid is on XY plane.
    float distanceOnGrid = length(worldPosition.xy - pushConstants.cameraPosition.xy);

    return clamp((distanceOnGrid - fadeStart) / (fadeEnd - fadeStart), 0.0, 1.0);
}

float cameraHeightFade()
{
    // Your grid plane is Z = 0, so camera height from grid is abs(camera.z).
    float heightFromGrid = abs(pushConstants.cameraPosition.z);

    // 1.0 near grid, 0.0 high above grid.
    return 1.0 - smoothstep(HeightFadeStart, HeightFadeEnd, heightFromGrid);
}

void main()
{
    vec2 viewportSize = max(pushConstants.gridSettings.zw, vec2(1.0));
    vec2 screenUv = gl_FragCoord.xy / viewportSize;

    vec3 nearWorld = reconstructWorld(screenUv, 0.0);
    vec3 farWorld = reconstructWorld(screenUv, 1.0);
    vec3 rayDirection = normalize(farWorld - nearWorld);

    // Grid plane is Z = 0.
    if (abs(rayDirection.z) < 0.0001) {
        discard;
    }

    float planeDistance = -nearWorld.z / rayDirection.z;
    if (planeDistance < 0.0) {
        discard;
    }

    vec3 worldPosition = nearWorld + rayDirection * planeDistance;

    float step = pushConstants.gridSettings.x;
    float majorStep = step * max(pushConstants.gridSettings.y, 1.0);

    float minor = gridLine(worldPosition.xy, step);
    float major = gridLine(worldPosition.xy, majorStep);
    float axisX = axisLine(worldPosition.y);
    float axisY = axisLine(worldPosition.x);

    vec4 color = MinorColor;
    color = mix(color, MajorColor, major);
    color = mix(color, AxisXColor, axisX);
    color = mix(color, AxisYColor, axisY);

    float t = radialFadeProgress(worldPosition);

    float minorFade = 1.0 - smoothstep(0.10, 0.55, t);
    float majorFade = 1.0 - smoothstep(0.35, 0.90, t);
    float axisFade = 1.0 - smoothstep(0.65, 1.00, t);

    float minorAlpha = minor * MinorColor.a * minorFade;
    float majorAlpha = major * MajorColor.a * majorFade;
    float axisAlphaX = axisX * AxisXColor.a * axisFade;
    float axisAlphaY = axisY * AxisYColor.a * axisFade;

    float alpha = max(minorAlpha, majorAlpha);
    alpha = max(alpha, axisAlphaX);
    alpha = max(alpha, axisAlphaY);

    // This makes grid disappear when camera flies upward on Z.
    alpha *= cameraHeightFade();

    if (alpha <= 0.001) {
        discard;
    }

    outColor = vec4(color.rgb, alpha);
}