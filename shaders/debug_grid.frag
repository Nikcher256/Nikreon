#version 450

layout(push_constant) uniform PushConstants {
    mat4 inverseViewProjection;
    vec4 cameraPosition;
    vec4 gridSettings;
    vec4 fadeSettings;
} pushConstants;

layout(location = 0) out vec4 outColor;

const vec4 MinorColor = vec4(0.30, 0.40, 0.52, 0.24);
const vec4 MajorColor = vec4(0.42, 0.54, 0.70, 0.42);
const vec4 AxisXColor = vec4(0.95, 0.32, 0.32, 0.82);
const vec4 AxisYColor = vec4(0.34, 0.86, 0.46, 0.82);

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

void main()
{
    vec2 viewportSize = max(pushConstants.gridSettings.zw, vec2(1.0));
    vec2 screenUv = gl_FragCoord.xy / viewportSize;
    vec3 nearWorld = reconstructWorld(screenUv, 0.0);
    vec3 farWorld = reconstructWorld(screenUv, 1.0);
    vec3 rayDirection = normalize(farWorld - nearWorld);

    if (abs(rayDirection.z) < 0.0001) {
        discard;
    }

    float planeDistance = -nearWorld.z / rayDirection.z;
    if (planeDistance < 0.0) {
        discard;
    }

    vec3 worldPosition = nearWorld + rayDirection * planeDistance;
    float cameraDistance = distance(worldPosition, pushConstants.cameraPosition.xyz);
    float fade = smoothstep(pushConstants.fadeSettings.y, pushConstants.fadeSettings.x, cameraDistance);
    if (fade <= 0.001) {
        discard;
    }

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

    float alpha = max(minor * MinorColor.a, major * MajorColor.a);
    alpha = max(alpha, axisX * AxisXColor.a);
    alpha = max(alpha, axisY * AxisYColor.a);
    outColor = vec4(color.rgb, alpha * fade);
}
