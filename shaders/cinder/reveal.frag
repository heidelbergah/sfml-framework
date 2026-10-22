// reveal.frag  -  Cinder
//
// The world is drawn in full colour, then this pass burns it back to ash
// everywhere it is cold. `heat` is a small, smoothed texture with a soft
// blob for every source of warmth: the player, the trail behind it, and
// each brazier that has been relit.
//
// Where the two meet, the edge behaves like burning paper: the threshold
// is pushed around by rising noise, a dark line of char sits on the cold
// side and a thin glowing line of ember on the warm side. Noise and
// dither are anchored to the world rather than the screen, so the edge
// does not swim when the camera moves.

uniform sampler2D texture;     // the world, in colour
uniform sampler2D heat;        // r = warmth, 0..1
uniform vec2  origin;          // world position of this frame's top-left pixel
uniform float time;
uniform float ember_flicker;   // 0..1, shifts the edge colour toward gold

float hash(vec2 p)
{
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float vnoise(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

float bayer2(vec2 a)
{
    a = floor(a);
    return fract(a.x / 2.0 + a.y * a.y * 0.75);
}

float bayer4(vec2 a)
{
    return bayer2(0.5 * a) * 0.25 + bayer2(a);
}

void main()
{
    vec2 uv    = gl_TexCoord[0].xy;
    vec2 world = floor(gl_FragCoord.xy) + origin;

    vec3  colour = texture2D(texture, uv).rgb;
    float warm   = texture2D(heat, uv).r;

    // The edge wavers: a slow rising field for the big shapes, a faster
    // one for the ragged detail, and a little ordered dither on top.
    float n = vnoise(world * 0.06 + vec2(0.0, time * 0.9)) * 0.65
            + vnoise(world * 0.21 - vec2(time * 0.4, time * 1.7)) * 0.35;
    float t = warm + (n - 0.5) * 0.22 + (bayer4(world) - 0.5) * 0.05;

    float threshold = 0.32;

    // Ash: the luminance of what is underneath, pressed down into the
    // dark end of the greys, so shapes stay readable but lifeless.
    float lum = dot(colour, vec3(0.299, 0.587, 0.114));
    vec3  ash = vec3(0.05 + lum * 0.24);

    vec3 ember = mix(vec3(0.847, 0.486, 0.290),      // orange glow
                     vec3(0.898, 0.655, 0.165),      // golden
                     clamp(n * 1.4 - 0.2 + ember_flicker * 0.4, 0.0, 1.0));

    vec3 result;
    if(t < threshold - 0.05)
    {
        result = ash;
    }
    else if(t < threshold)
    {
        result = ash * 0.35;                          // char
    }
    else if(t < threshold + 0.035)
    {
        result = ember;                               // the burning line
    }
    else
    {
        // Colour, a touch warmed just inside the edge.
        float glow = 1.0 - smoothstep(threshold, threshold + 0.25, t);
        result = colour + vec3(0.10, 0.04, 0.0) * glow;
    }

    gl_FragColor = vec4(result, 1.0);
}
